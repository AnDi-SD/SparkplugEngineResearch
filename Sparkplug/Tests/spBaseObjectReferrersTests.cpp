#include "Code/SparkBase/spBaseObject.h"
#include "Code/SparkBase/spBaseObjectReferrers.h"
#include "Analysis/PC/SparkBaseAbi.h"
#include <array>
#include <cstdlib>
#include <iostream>
#include <new>
#include <stdexcept>
#include <string>
#include <type_traits>

namespace
{
    struct Allocation final{void* pointer=nullptr;unsigned releases=0;};
    std::array<Allocation,64> metadata{};unsigned metadataCount=0;
    std::array<Allocation,4> objects{};
    bool trackMetadata=false,trackObjects=false,failNextAllocation=false;
    void Allocated(void* value) noexcept
    {
        if(!trackMetadata)return;
        if(metadataCount==metadata.size())std::terminate();
        metadata[metadataCount++]={value,0};
    }
    void Released(void* value) noexcept
    {
        if(trackMetadata)for(unsigned i=0;i<metadataCount;++i)if(metadata[i].pointer==value)++metadata[i].releases;
        if(trackObjects)for(auto& item:objects)if(item.pointer==value)++item.releases;
    }
}
void* operator new(std::size_t bytes)
{
    if(failNextAllocation){failNextAllocation=false;throw std::bad_alloc();}
    if(void* value=std::malloc(bytes?bytes:1)){Allocated(value);return value;}
    throw std::bad_alloc();
}
void operator delete(void* value) noexcept{Released(value);std::free(value);}
void operator delete(void* value,std::size_t) noexcept{::operator delete(value);}
void* operator new[](std::size_t bytes){return ::operator new(bytes);}
void operator delete[](void* value) noexcept{::operator delete(value);}
void operator delete[](void* value,std::size_t) noexcept{::operator delete(value);}

using namespace sparkplug::reconstruction;
namespace
{
    using Profile=spBaseReferrerProfileForAnalysis;
    using Result=spBaseReferrerResultForAnalysis;
    void Check(bool value,const char* text){if(!value)throw std::runtime_error(text);}
    bool MetadataFreedOnce() noexcept
    {for(unsigned i=0;i<metadataCount;++i)if(metadata[i].releases!=1)return false;return true;}
    void Replay()
    {
        unsigned count,deleting;
        while(std::cin>>count>>deleting)
        {
            Check(count<=4&&deleting<=1,"Bounded referrer input");
            std::array<unsigned,4> sequence{};
            for(auto& value:sequence){std::cin>>value;Check(bool(std::cin)&&value<=3,"Complete payload identities");}
            std::array<std::unique_ptr<spNamedObject>,3> payloads;
            for(auto& value:payloads)value=std::make_unique<spNamedObject>();
            std::unique_ptr<spNamedObject> ownedSubject;void* external=nullptr;spNamedObject* subject;
            if(deleting){ownedSubject=std::make_unique<spNamedObject>();subject=ownedSubject.get();}
            else{external=::operator new(sizeof(spNamedObject));subject=new(external)spNamedObject();}
            for(unsigned i=0;i<3;++i)objects[i]={payloads[i].get(),0};objects[3]={subject,0};
            trackObjects=true;
            spBaseReferrerContextForAnalysis context(*subject,Profile::StableBorrowedBaseObjects);
            Check(!context.HasListForAnalysis()&&context.GetCountForAnalysis()==0,"Initial optional state is absent");
            std::array<std::array<unsigned,5>,4> steps{};
            metadata={};metadataCount=0;trackMetadata=true;
            for(unsigned step=0;step<count;++step)
            {
                spBaseObject* value=sequence[step]==3?subject:payloads[sequence[step]].get();
                Check(AppendBaseReferrerForAnalysis(*subject,*value,context)==Result::Completed,"Whole admitted append");
                const auto size=context.GetCountForAnalysis();Check(size==step+1&&size<=4,"Append count");
                steps[step][0]=static_cast<unsigned>(size);
                for(unsigned i=0;i<size;++i)
                {
                    auto* item=context.GetReferrerForAnalysis(i);unsigned identity=4;
                    if(item==subject)identity=3;
                    else for(unsigned j=0;j<3;++j)if(item==payloads[j].get())identity=j;
                    Check(identity<=3,"Borrowed payload identity");steps[step][i+1]=identity;
                }
            }
            const auto finalCount=context.GetCountForAnalysis();const bool hadList=context.HasListForAnalysis();
            Check(CloseBaseReferrersForAnalysis(*subject,context)==Result::Completed,"Terminal metadata cleanup");
            trackMetadata=false;
            const bool metadataFreed=MetadataFreedOnce();
            Check(metadataFreed&&(metadataCount>0)==(count>0),"Real host metadata released once");
            Check(!context.HasListForAnalysis()&&context.IsClosedForAnalysis()&&context.GetCountForAnalysis()==0,"Terminal represented state");
            if(deleting)ownedSubject.reset();else subject->~spNamedObject();
            const unsigned objectFreedByDtor=objects[3].releases;
            Check(objectFreedByDtor==deleting,"Deleting and external subject storage roles");
            bool externalAlive=true;
            for(unsigned i=0;i<3;++i)
            {
                Check(objects[i].releases==0,"Metadata cleanup must not delete a borrowed external object");
                externalAlive=externalAlive&&payloads[i]->GetName()==nullptr;
            }
            if(external)::operator delete(external);
            Check(objects[3].releases==1&&externalAlive,"Subject freed once, three external payloads survive");
            trackObjects=false;
            // No access to the destroyed subject. Context queries above only
            // read its explicit metadata state; its stored binding is not used.
            std::cout<<finalCount;
            for(unsigned i=0;i<count;++i)for(unsigned j=0;j<=steps[i][0];++j)std::cout<<' '<<steps[i][j];
            std::cout<<' '<<hadList<<" 0 "<<externalAlive<<' '<<objectFreedByDtor<<" 1 "<<metadataFreed<<'\n';
        }
        Check(std::cin.eof(),"Complete referrer replay input");
    }
    void Contracts()
    {
        static_assert(!std::is_copy_constructible_v<spBaseReferrerContextForAnalysis>);
        static_assert(!std::is_move_constructible_v<spBaseReferrerContextForAnalysis>);
        spNamedObject subject,payload,other;
        spBaseReferrerContextForAnalysis unqualified(subject);
        Check(AppendBaseReferrerForAnalysis(subject,payload,unqualified)==Result::UnqualifiedContext
            &&CloseBaseReferrersForAnalysis(subject,unqualified)==Result::UnqualifiedContext
            &&!unqualified.HasListForAnalysis(),"Default profile refuses before effects");
        spBaseReferrerContextForAnalysis context(subject,Profile::StableBorrowedBaseObjects);
        Check(AppendBaseReferrerForAnalysis(other,payload,context)==Result::WrongSubject
            &&!context.HasListForAnalysis(),"State cannot be rebound by a different receiver");
        for(unsigned i=0;i<4;++i)Check(AppendBaseReferrerForAnalysis(subject,payload,context)==Result::Completed,"Duplicate pointers stay distinct");
        Check(AppendBaseReferrerForAnalysis(subject,other,context)==Result::OutsideProfile
            &&context.GetCountForAnalysis()==4&&context.GetReferrerForAnalysis(3)==&payload,"Oversized profile refuses without consuming input");
        Check(CloseBaseReferrersForAnalysis(other,context)==Result::WrongSubject
            &&context.GetCountForAnalysis()==4,"Wrong terminal receiver refuses before effects");
        Check(CloseBaseReferrersForAnalysis(subject,context)==Result::Completed
            &&AppendBaseReferrerForAnalysis(subject,payload,context)==Result::ClosedContext
            &&CloseBaseReferrersForAnalysis(subject,context)==Result::ClosedContext,"Closed context cannot restart native object lifetime");
        metadata={};metadataCount=0;trackMetadata=true;
        {spBaseReferrerContextForAnalysis abandoned(subject,Profile::StableBorrowedBaseObjects);
            Check(AppendBaseReferrerForAnalysis(subject,payload,abandoned)==Result::Completed,"Host fallback input");}
        trackMetadata=false;Check(metadataCount>0&&MetadataFreedOnce(),"Abandoned context releases host metadata");
        spBaseReferrerContextForAnalysis failed(subject,Profile::StableBorrowedBaseObjects);
        bool threw=false;failNextAllocation=true;
        try{(void)AppendBaseReferrerForAnalysis(subject,payload,failed);}catch(const std::bad_alloc&){threw=true;}
        Check(threw&&failed.IsPoisonedForAnalysis()
            &&AppendBaseReferrerForAnalysis(subject,payload,failed)==Result::UnqualifiedContext,
            "Host allocation failure poisons context; no native rollback or retry claim");
    }
}
int main(int argc,char** argv)
{
    try
    {
        if(argc==2&&std::string(argv[1])=="--replay")Replay();
        else{Contracts();std::cout<<"Base referrer component contracts passed\n";}
        return 0;
    }
    catch(const std::exception& error){trackMetadata=trackObjects=false;std::cerr<<error.what()<<'\n';return 1;}
}
