#include "GuestSceneTransition.h"
#include "GuestIsolation.h"
#include "PinnedImage.h"

namespace winxcoop::adapter
{
    namespace
    {
        constexpr std::uint32_t WorldReturn=0x45A802, WorldCaller=0x41CDF8;
        constexpr std::uint32_t FlowGlobal=0x755294, FlowTable=0x702D2C, Request=0x595450;
        constexpr DWORD FullContext=CONTEXT_CONTROL|CONTEXT_INTEGER|CONTEXT_SEGMENTS|
            CONTEXT_FLOATING_POINT|CONTEXT_DEBUG_REGISTERS|CONTEXT_EXTENDED_REGISTERS;
        bool Read(std::uint32_t at,void* out,SIZE_T bytes) noexcept
        {SIZE_T got{};return at>=0x10000 && ReadProcessMemory(GetCurrentProcess(),reinterpret_cast<void*>(at),out,bytes,&got) && got==bytes;}
        std::uint32_t Word(std::uint32_t at) noexcept
        {std::uint32_t value{};Read(at,&value,4);return value;}
        bool Equal(const BYTE* a,const BYTE* b,SIZE_T bytes) noexcept
        {for(SIZE_T i=0;i<bytes;++i)if(a[i]!=b[i])return false;return true;}
        bool Stack(std::uint32_t at,SIZE_T bytes) noexcept
        {
            MEMORY_BASIC_INFORMATION r{};
            return at>=0x10000 && !(at&3) && VirtualQuery(reinterpret_cast<void*>(at),&r,sizeof(r)) &&
                r.State==MEM_COMMIT && r.Type==MEM_PRIVATE && !(r.Protect&PAGE_GUARD) &&
                (r.Protect&0xFF)==PAGE_READWRITE &&
                static_cast<std::uint64_t>(at)+bytes<=reinterpret_cast<std::uintptr_t>(r.BaseAddress)+r.RegionSize;
        }
        void Trace(const char* prefix,std::uint32_t level,std::uint32_t observedEip,std::uint32_t value) noexcept
        {
            char line[220]{};char* p=line;char* const end=line+sizeof(line)-1;
            const auto append=[&](const char* s) noexcept {while(*s && p<end)*p++=*s++;};
            const auto word=[&](const char* s,std::uint32_t v) noexcept
            {append(s);for(unsigned n=0;n<8 && p<end;++n)*p++="0123456789ABCDEF"[(v>>(28-n*4))&15];};
            append(prefix);word(" level=",level);word(" observedEip=",observedEip);word(" opaqueResult=",value);
            append(" requestOnly=true nativeLoadCompleted=false");OutputDebugStringA(line);
        }
    }
    bool GuestSceneTransition::Fail(const char* reason) noexcept
    {if(!failed_){failed_=true;failure_=reason;}return false;}

    bool GuestSceneTransition::Prepare(PinnedImage& image,GuestIsolation& isolation,DWORD thread) noexcept
    {
        const BYTE entry[]={0x56,0x57,0x8B,0x7C,0x24,0x0C};
        const BYTE returned[]={0x5F,0x5E,0xC2,0x08,0x00};
        if(prepared_ || failed_ || !thread || thread!=GetCurrentThreadId() ||
            !image.VerifyFeatureCode(Request,entry,sizeof(entry)) ||
            !image.VerifyFeatureCode(0x5954A1,returned,sizeof(returned)))
            return Fail("guest transition preparation requires original source/live RET8 request before hooks");
        image_=&image;isolation_=&isolation;thread_=thread;prepared_=true;return true;
    }

    bool GuestSceneTransition::PageMatches() const noexcept
    {
        MEMORY_BASIC_INFORMATION region{};BYTE actual[sizeof(code_)]{};
        return page_ && VirtualQuery(page_,&region,sizeof(region)) && region.State==MEM_COMMIT &&
            region.Type==MEM_PRIVATE && !(region.Protect&PAGE_GUARD) &&
            (region.Protect&0xFF)==PAGE_EXECUTE_READ && region.RegionSize>=4096 &&
            Read(reinterpret_cast<std::uint32_t>(page_),actual,sizeof(actual)) && Equal(actual,code_,sizeof(code_));
    }

    bool GuestSceneTransition::CanBeginAtWorld(const CONTEXT& context,std::uint32_t targetLevel,std::uint32_t targetEntry) const noexcept
    {
        return prepared_ && !failed_ && !pending_ && !retired_ && !page_ && GetCurrentThreadId()==thread_ &&
            !targetEntry && targetLevel>=1 && targetLevel<=50 && context.Eip==WorldReturn &&
            (context.ContextFlags&FullContext)==FullContext && !(context.EFlags&0x100) &&
            isolation_ && isolation_->CanFollow(targetLevel);
    }

    bool GuestSceneTransition::BeginAtWorld(CONTEXT& context,std::uint32_t targetLevel,std::uint32_t targetEntry) noexcept
    {
        if(!CanBeginAtWorld(context,targetLevel,targetEntry) || !prepared_ || failed_ || pending_ || retired_ || page_ || GetCurrentThreadId()!=thread_ ||
            targetLevel<1 || targetLevel>50 || targetEntry || context.Eip!=WorldReturn ||
            (context.ContextFlags&FullContext)!=FullContext || (context.EFlags&0x100) ||
            context.Esp<0x10040 || context.Esp>0xFFFFFFFB || !Stack(context.Esp-12,16) || Word(context.Esp)!=WorldCaller)
            return Fail("guest transition left its single full-context original World RET boundary");
        const auto flow=Word(FlowGlobal);
        if(flow<0x10000 || (flow&3) || Word(flow)!=FlowTable)
            return Fail("guest transition lost its initialized native Flow owner");
        page_=static_cast<BYTE*>(VirtualAlloc(nullptr,4096,MEM_RESERVE|MEM_COMMIT,PAGE_READWRITE));
        if(!page_)return Fail("guest transition private call page allocation failed");
        // PUSH raw1; PUSH level; MOV ECX,Flow; MOV EAX,595450; CALL EAX; INT3.
        // The second argument is the historical raw request argument, NOT an
        // entrance/placement ID. No Profile/save/start-position field is written.
        const std::uint32_t words[]={1,targetLevel,flow,Request};
        const BYTE ops[]={0x68,0x68,0xB9,0xB8};
        for(unsigned i=0;i<4;++i){code_[i*5]=ops[i];CopyMemory(code_+i*5+1,&words[i],4);}
        code_[20]=0xFF;code_[21]=0xD0;code_[22]=0xCC;
        CopyMemory(page_,code_,sizeof(code_));DWORD previous{};
        if(!VirtualProtect(page_,4096,PAGE_EXECUTE_READ,&previous) || previous!=PAGE_READWRITE ||
            !FlushInstructionCache(GetCurrentProcess(),page_,sizeof(code_)) || !PageMatches())
            return Fail("guest transition private call RX seal/readback failed");
        trap_=reinterpret_cast<std::uint32_t>(page_)+22;
        if(!isolation_->PrepareFollow(targetLevel,flow,trap_,context.Esp-12))
            return Fail(isolation_->Failure());
        CopyMemory(&saved_,&context,sizeof(saved_));originalReturn_=Word(context.Esp);
        flow_=flow;target_=targetLevel;began_=GetTickCount64();pending_=true;
        context.Eip=reinterpret_cast<std::uint32_t>(page_);
        Trace("WinxCoop guest follow call scheduled outside VEH",target_,WorldReturn,0);
        return true;
    }

    bool GuestSceneTransition::Handles(std::uint32_t exceptionAddress) const noexcept
    {return pending_ && page_ && exceptionAddress==trap_;}

    bool GuestSceneTransition::Complete(CONTEXT& context) noexcept
    {
        const auto now=GetTickCount64();
        if(!prepared_ || failed_ || !pending_ || retired_ || GetCurrentThreadId()!=thread_ ||
            (context.Eip!=trap_ && context.Eip!=trap_+1) || context.Esp!=saved_.Esp ||
            (context.ContextFlags&FullContext)!=FullContext || (context.EFlags&0x100) ||
            now<began_ || now-began_>10000 || !PageMatches() ||
            Word(saved_.Esp)!=originalReturn_ || Word(FlowGlobal)!=flow_ || Word(flow_)!=FlowTable ||
            !isolation_->CompleteFollowRequest(target_))
            return Fail("guest transition completion lost exact request/frame/queue ownership");
        const auto observedEip=context.Eip,result=context.Eax;
        pending_=false;retired_=true;completedAt_=now;
        CopyMemory(&context,&saved_,sizeof(context));context.Eip=originalReturn_;context.Esp=saved_.Esp+4;
        Trace("WinxCoop guest follow original RET8 completed full-context restored",target_,observedEip,result);
        return true;
    }

    bool GuestSceneTransition::OnLaterWorld(CONTEXT& context) noexcept
    {
        if(failed_)return false;
        if(!retired_)return !pending_ || Fail("new World arrived during an unmatched guest native request");
        if(!prepared_ || pending_ || GetCurrentThreadId()!=thread_ || context.Eip!=WorldReturn ||
            Word(context.Esp)!=WorldCaller || !PageMatches() || GetTickCount64()<completedAt_)
            return Fail("guest transition call page lacks its later original World release witness");
        const auto first=reinterpret_cast<std::uint32_t>(page_);
        const std::uint32_t pointers[]={context.Eip,context.Esp,context.Eax,context.Ebx,context.Ecx,context.Edx,
            context.Esi,context.Edi,context.Ebp,Word(context.Esp)};
        for(const auto value:pointers)
            if(value>=first && static_cast<std::uint64_t>(value)<static_cast<std::uint64_t>(first)+4096)
                return Fail("guest transition later World still references the private call page");
        if(!VirtualFree(page_,0,MEM_RELEASE))return Fail("guest transition retired private call page release failed");
        page_=nullptr;trap_=0;retired_=false;
        Trace("WinxCoop guest follow private page released at later World",target_,WorldReturn,0);
        return true;
    }
}
