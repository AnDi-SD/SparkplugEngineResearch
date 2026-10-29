#include "Code/SparkplugDX/spDXAudioBankEntry.h"
#include "Analysis/PC/spAudioBankEntryAbi.h"
#include <array>
#include <cstdlib>
#include <cstring>
#include <iostream>
#include <new>
#include <stdexcept>
#include <string>
#include <type_traits>

// Actual host releases are observed separately from native pool count bytes,
// addresses and global contexts. Long names force a real character allocation.
namespace
{
    struct Watched final{const void* address=nullptr;unsigned releases=0;};
    std::array<Watched,4> watched{};unsigned watchedCount=0;
    void Released(void* value) noexcept
    {for(unsigned i=0;i<watchedCount;++i)if(value==watched[i].address)++watched[i].releases;}
}
void* operator new(std::size_t bytes)
{if(void* value=std::malloc(bytes?bytes:1))return value;throw std::bad_alloc();}
void operator delete(void* value) noexcept{Released(value);std::free(value);}
void operator delete(void* value,std::size_t) noexcept{::operator delete(value);}
void* operator new[](std::size_t bytes){return ::operator new(bytes);}
void operator delete[](void* value) noexcept{::operator delete(value);}
void operator delete[](void* value,std::size_t) noexcept{::operator delete(value);}

using namespace sparkplug::reconstruction;
namespace
{
    using Profile=spAudioBankEntry::CapturedEmptyProfileForAnalysis;
    constexpr auto Captured=Profile::NonzeroUnassignedE0WithNullResources;
    using State=std::array<std::uint32_t,24>;
    constexpr State Defaults{0,1};
    unsigned checks=0;
    void Check(bool value,const char* why){++checks;if(!value)throw std::runtime_error(why);}
    struct NameOwner final:spNamedObject
    {void ShareTo(spNamedObject& destination)const noexcept{CopyNameToForAnalysis(destination);}};
    const char* Names[]={nullptr,
        "audio-entry-retained-name-A-with-real-shared-owner-over-forty-eight-characters",
        "audio-entry-retained-name-B-with-distinct-live-owner-over-forty-eight-characters"};
    State Capture(const spDXAudioBankEntry& value)
    {
        const auto& base=value.GetStateForAnalysis();const auto& dx=value.GetDxStateForAnalysis();
        State out{base.rawByte14,base.rawWord18,base.rawWord1C,base.rawWord20,base.rawWord24,
            base.rawByte28,base.rawWord2C,base.rawByte30};
        for(unsigned i=0;i<11;++i)out[8+i]=dx.rawWords34To5C[i];
        out[19]=dx.rawWord64;out[20]=dx.rawWord68;out[21]=dx.rawWord94;out[22]=dx.rawWordE4;out[23]=dx.rawWordF4;
        return out;
    }
    void Watch(unsigned index,const void* address)
    {Check(index<watched.size()&&address,"Live allocation witness");watched[index]={address,0};if(watchedCount<=index)watchedCount=index+1;}
    void Print(const State& value){for(auto word:value)std::cout<<word<<' ';}
    void Replay()
    {
        unsigned mode,sourceName,destName,deletingFlag;std::uint32_t raw18,dest18;
        while(std::cin>>mode>>sourceName>>destName>>deletingFlag>>raw18>>dest18)
        {
            Check(mode<=3&&sourceName<=1&&destName<=2&&deletingFlag<=1,"Bounded Entry replay input");
            std::array<std::unique_ptr<NameOwner>,2> names;
            void* external=nullptr;std::unique_ptr<spDXAudioBankEntry> sourceOwner;
            spDXAudioBankEntry* source=nullptr;
            if(deletingFlag){sourceOwner=std::make_unique<spDXAudioBankEntry>(Captured);source=sourceOwner.get();}
            else {external=::operator new(sizeof(spDXAudioBankEntry));source=new(external)spDXAudioBankEntry(Captured);}
            const auto initial=Capture(*source);Check(initial==Defaults,"All24 proven constructor own fields");
            const auto assign=[&](spDXAudioBankEntry& value,unsigned identity)
            {
                if(!identity)return;
                if(!names[identity-1]){names[identity-1]=std::make_unique<NameOwner>();names[identity-1]->SetName(Names[identity]);}
                names[identity-1]->ShareTo(value);
            };
            assign(*source,sourceName);source->SetRawWord18ForFixture(raw18);
            std::unique_ptr<spBaseObject> otherOwner;spDXAudioBankEntry* other=nullptr;
            if(mode==1)
            {auto value=std::make_unique<spDXAudioBankEntry>(Captured);assign(*value,destName);value->SetRawWord18ForFixture(dest18);other=value.get();otherOwner=std::move(value);}
            const auto before=Capture(*source);const auto otherBefore=other?Capture(*other):State{};
            spCloneManager clones;bool returned=true,mapped=false;
            if(mode==1||mode==2)returned=source->vfunc_14(mode==1?*other:*source,clones);
            else if(mode==3)
            {otherOwner=source->vfunc_10(clones);other=dynamic_cast<spDXAudioBankEntry*>(otherOwner.get());returned=other!=nullptr;mapped=other&&clones.FindClone(*source)==other;}
            Check(before==Capture(*source),"Operation preserves source own payload");
            Check(!other||(mode==3?Capture(*other)==Defaults:Capture(*other)==otherBefore),"Copy preserves destination payload; Clone has fresh defaults");
            const auto identify=[&](const spDXAudioBankEntry* value)
            {
                if(!value||!value->GetName())return 0u;
                for(unsigned i=0;i<2;++i)if(names[i]&&value->GetName()==names[i]->GetName())return i+1;
                throw std::runtime_error("Entry must share the actual retained Name allocation");
            };
            const auto sourceAfter=Capture(*source),otherAfter=other?Capture(*other):State{};
            const unsigned sourceNameAfter=identify(source),otherNameAfter=identify(other);const bool hasOther=other!=nullptr;
            Check(sourceNameAfter==(mode==2?0u:sourceName),"Self Copy clears name, other paths preserve source name");
            Check(!other||otherNameAfter==sourceName,"Copy and Clone share source Name");
            watched={};watchedCount=0;Watch(0,source);if(other)Watch(1,other);
            for(unsigned i=0;i<2;++i)if(names[i])Watch(i+2,names[i]->GetName());
            otherOwner.reset();
            if(deletingFlag)sourceOwner.reset();else source->~spDXAudioBankEntry();
            const auto objectReleases=std::array<unsigned,2>{watched[0].releases,watched[1].releases};
            Check(objectReleases[0]==deletingFlag&&objectReleases[1]==unsigned(hasOther),"Owned Entry released once; external object retained by destructor0");
            for(unsigned i=0;i<2;++i)if(names[i])
            {Check(watched[i+2].releases==0&&std::strcmp(names[i]->GetName(),Names[i+1])==0,"External shared Name owner survives intact");names[i].reset();Check(watched[i+2].releases==1,"Actual host last Name allocation released once after Entry lifetime");}
            if(external)
            {::operator delete(external);Check(watched[0].releases==1,"External caller releases dead-object storage exactly once");}
            watchedCount=0;
            Print(initial);Print(sourceAfter);Print(otherAfter);
            // Final two tokens qualify represented source payload and the
            // existing/fresh destination payload roles. No dead object read.
            std::cout<<sourceNameAfter<<' '<<otherNameAfter<<' '<<returned<<' '<<mapped<<' '
                <<objectReleases[0]<<' '<<objectReleases[1]<<" 1 "<<hasOther<<'\n';
        }
        Check(std::cin.eof(),"Complete Entry replay input");
    }
    struct BaseBinding final:spAudioBankEntry{BaseBinding():spAudioBankEntry(Captured){}};
    void Contracts()
    {
        static_assert(!std::is_default_constructible_v<spDXAudioBankEntry>);
        Check(spAudioBankEntry::StaticRTTI().base==&spBaseObject::StaticRTTI()
            &&spDXAudioBankEntry::StaticRTTI().base==&spAudioBankEntry::StaticRTTI(),"Registration ancestry independent of physical Named prefix");
        Check(!spAudioBankEntry::StaticRTTI().factory&&!spDXAudioBankEntry::StaticRTTI().factory,"No implicit portable factory can infer captured native E0");
        Check(!spRTTIManager::Instance().Create(spDXAudioBankEntry::ClassID),"Unbound portable RTTI factory refuses");
        spDXAudioBankEntry source(Captured);Check(Capture(source)==Defaults&&!source.GetName(),"Proven fields and null physical name");
        Check(source.IsKindOf(spAudioBankEntry::ClassID)&&source.IsKindOf(spBaseObject::ClassID)&&!source.IsKindOf(spNamedObject::ClassID),"Engine RTTI Named remains false");
        NameOwner owner;owner.SetName(Names[1]);owner.ShareTo(source);source.SetRawWord18ForFixture(0xFFFFFFFF);
        spCloneManager clones;auto cloneBase=source.vfunc_10(clones);auto* clone=dynamic_cast<spDXAudioBankEntry*>(cloneBase.get());
        Check(clone&&Capture(*clone)==Defaults&&clone->GetName()==source.GetName()&&clones.FindClone(source)==clone,"Typed Clone defaults/shared Name/real portable map");
        Check(source.vfunc_14(source,clones)&&!source.GetName()&&owner.GetName()==clone->GetName(),"Self-Copy release-before-source-reread quirk");
        spNamedObject unrelated("unqualified destination");Check(!source.vfunc_14(unrelated,clones)&&std::strcmp(unrelated.GetName(),"unqualified destination")==0,"Foreign destination analytically refused before effects");
        bool refused=false;try{BaseBinding base;(void)base.vfunc_10(clones);}catch(const std::logic_error&){refused=true;}
        Check(refused,"Base Entry Clone is explicitly unqualified");
        refused=false;try{spDXAudioBankEntry invalid(static_cast<Profile>(255));}catch(const std::logic_error&){refused=true;}
        Check(refused,"Invalid captured profile refuses");
    }
}
int main(int argc,char** argv)
{
    try
    {
        if(argc==2&&std::string(argv[1])=="--replay")Replay();
        else {Contracts();std::cout<<"AudioBankEntry captured empty PC contracts passed checks="<<checks<<'\n';}
        return 0;
    }
    catch(const std::exception& error){std::cerr<<"AudioBankEntry "<<error.what()<<'\n';return 1;}
}
