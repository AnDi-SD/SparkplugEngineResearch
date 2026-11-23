#include "GuestFlowGate.h"
#include "PinnedImage.h"
#include <cstddef>

namespace winxcoop::adapter
{
    namespace
    {
        constexpr std::uint32_t Queue = 0x594E80, Push = 0x5953E0, Replace = 0x595450;
        constexpr std::uint32_t FlowGlobal = 0x755294, FlowTable = 0x702D2C;
        constexpr std::uint32_t LogoTable = 0x70ABB4, PauseTable = 0x710398;
        constexpr std::uint32_t MainMenuTable = 0x7102A8, DiaryTable = 0x70ED30;
        constexpr std::uint32_t OptionTable = 0x70FD38, OptionGetter = 0x5E5310;
        constexpr std::uint32_t OptionRecord = 0x768750, OptionClass = 0xAC2154CB;
        struct Pin final
        {
            std::uint32_t address, size;
            BYTE bytes[32];
        };
        // Complete original instruction spans, not a whole-executable hash.
        // The queue writes ten {target, operation, countdown} slots at
        // Flow+1B8, stride0C. Native operation0/1/2/3 means push/pop/replace/
        // return-to-retained; operation4 prepares resources. Countdown is
        // decremented by original5967E0, not interpreted as wall-clock time.
        constexpr Pin Pins[] = {
            {Queue, 8, {0x33,0xC0,0x8D,0x91,0xB8,0x01,0x00,0x00}},
            {Push, 6, {0x56,0x57,0x8B,0x7C,0x24,0x0C}},
            {Replace, 6, {0x56,0x57,0x8B,0x7C,0x24,0x0C}},
            {0x595426, 5, {0x5F,0x5E,0xC2,0x08,0x00}},
            {0x5954A1, 5, {0x5F,0x5E,0xC2,0x08,0x00}},
            {0x5954B0, 5, {0x5F,0x5E,0xC2,0x08,0x00}},
            {0x594EEE, 10, {0x89,0x8A,0xC0,0x01,0x00,0x00,0x5E,0xC2,0x0C,0x00}},
            {0x5CDF87, 10, {0x6A,0x00,0x52,0x8B,0xC8,0xE8,0x4F,0x74,0xFC,0xFF}},
            {0x40E5EF, 11, {0x6A,0x01,0x6A,0x36,0x8B,0xC8,0xE8,0xE6,0x6D,0x18,0x00}},
            {0x59541A, 12, {0x6A,0x02,0x6A,0x00,0x8B,0xCE,0x57,0xE8,0x5A,0xFA,0xFF,0xFF}},
            {0x59542B, 12, {0x6A,0x01,0x6A,0x00,0x8B,0xCE,0x57,0xE8,0x49,0xFA,0xFF,0xFF}},
            // Exact inner tuples of one owned original595450(level,raw1).
            {0x59545D, 11, {0x6A,0x00,0x6A,0x01,0x6A,0x51,0xE8,0x18,0xFA,0xFF,0xFF}},
            {0x595489, 12, {0x6A,0x00,0x6A,0x04,0x57,0x8B,0xCE,0xE8,0xEB,0xF9,0xFF,0xFF}},
            {0x595495, 12, {0x6A,0x02,0x6A,0x00,0x8B,0xCE,0x57,0xE8,0xDF,0xF9,0xFF,0xFF}},
            {0x5954A6, 10, {0x6A,0x01,0x6A,0x02,0x57,0xE8,0xD0,0xF9,0xFF,0xFF}},
            {0x5954C0, 14, {0x8B,0x44,0x24,0x04,0x6A,0x01,0x6A,0x03,0x50,0xE8,0xB2,0xF9,0xFF,0xFF}},
            {0x595440, 11, {0x6A,0x01,0x6A,0x01,0x6A,0x51,0xE8,0x35,0xFA,0xFF,0xFF}},
            {0x5F00DC, 11, {0xE8,0x7F,0x11,0x00,0x00,0xC7,0x06,0x98,0x03,0x71,0x00}},
            {0x5F0080, 6, {0xB8,0x50,0x8A,0x76,0x00,0xC3}},
            {0x5CDD70, 6, {0xB8,0x18,0x7C,0x76,0x00,0xC3}},
            {0x5EF690, 6, {0xB8,0x90,0x89,0x76,0x00,0xC3}},
            {0x5D53C0, 6, {0xB8,0xD8,0x7F,0x76,0x00,0xC3}},
            // Diary's Back command replaces its page with Option66. The
            // ordinary exit prompt lives inside this exact menu type; it is
            // not a new playable scene or a blanket admission of pages62..66.
            {OptionGetter, 6, {0xB8,0x50,0x87,0x76,0x00,0xC3}},
            {0x5D60BF, 10, {0x53,0x6A,0x42,0x8B,0xC8,0xE8,0x87,0xF3,0xFB,0xFF}},
            {0x596509, 10, {0x8B,0x96,0xB8,0x01,0x00,0x00,0x8B,0x4C,0x96,0x18}},
        };
        bool Read(const std::uint32_t address, void* value, const SIZE_T size) noexcept
        {
            SIZE_T got{};
            return address>=0x10000 && static_cast<std::uint64_t>(address)+size<=0x100000000ULL &&
                ReadProcessMemory(GetCurrentProcess(),reinterpret_cast<const void*>(address),value,size,&got) && got==size;
        }
        bool Word(const std::uint32_t address, std::uint32_t& value) noexcept
        { return Read(address,&value,sizeof(value)); }
        bool Same(const BYTE* a, const BYTE* b, const std::size_t size) noexcept
        {
            for(std::size_t i=0;i<size;++i)if(a[i]!=b[i])return false;
            return true;
        }
        bool StackRange(const std::uint32_t address, const std::uint32_t size) noexcept
        {
            MEMORY_BASIC_INFORMATION region{};
            return address>=0x10000 && !(address&3) &&
                VirtualQuery(reinterpret_cast<const void*>(address),&region,sizeof(region)) &&
                region.State==MEM_COMMIT && region.Type==MEM_PRIVATE && !(region.Protect&PAGE_GUARD) &&
                (region.Protect&0xFF)==PAGE_READWRITE &&
                static_cast<std::uint64_t>(address)+size<=reinterpret_cast<std::uintptr_t>(region.BaseAddress)+region.RegionSize;
        }
        void Append(char*& cursor, const char* end, const char* value) noexcept
        { while(*value && cursor<end)*cursor++=*value++; }
        void Hex(char*& cursor, const char* end, const char* label, const std::uint32_t value) noexcept
        {
            Append(cursor,end,label);
            for(unsigned i=0;i<8 && cursor<end;++i)*cursor++="0123456789ABCDEF"[(value>>(28-i*4))&15];
        }
        void TraceBlock(const CONTEXT& context, const std::uint32_t caller, const std::uint32_t target,
            const std::uint32_t mode, const std::uint32_t delay) noexcept
        {
            char text[240]{};char* cursor=text;const char* end=text+sizeof(text)-1;
            Append(cursor,end,"WinxCoop guest flow first-block");
            Hex(cursor,end," entry=",context.Eip);Hex(cursor,end," caller=",caller);
            Hex(cursor,end," target=",target);Hex(cursor,end," mode=",mode);Hex(cursor,end," countdown=",delay);
            Append(cursor,end," pendingQueueWrites=0 nativeResultOpaque=true");OutputDebugStringA(text);
        }
    }

    bool GuestFlowGate::Fail(const char* reason, const std::uint32_t address) noexcept
    {
        if(!failed_){failed_=true;failure_=reason;failedAddress_=address;}
        return false;
    }
    bool GuestFlowGate::Prepare(PinnedImage& image, const std::uint32_t assigned) noexcept
    {
        if(prepared_ || installAttempted_ || failed_ || !assigned || assigned>50)
            return Fail("guest flow preparation is single-use and requires an assigned playable level");
        image_=&image;assigned_=assigned;
        for(const auto& pin:Pins)
            if(!image.VerifyFeatureCode(pin.address,pin.bytes,pin.size))
                return Fail("guest flow source/live instruction pin differs",pin.address);
        // Preparation precedes the original CRT. Require the fixed physical
        // table/getter now, but allow its still-zero RTTI record. Every actual
        // Option request below requires the initialized native class identity.
        if(!image.VerifyFeatureType(OptionTable,OptionClass,true))
            return Fail("guest Option menu source/live physical type differs",OptionTable);
        prepared_=true;return true;
    }
    bool GuestFlowGate::Install() noexcept
    {
        if(!prepared_ || installAttempted_ || failed_)return Fail("guest flow installation is not prepared or was already attempted");
        installAttempted_=true;
        for(unsigned i=0;i<3;++i)
        {
            const auto& pin=Pins[i];
            if(!image_->VerifyFeatureCode(pin.address,pin.bytes,pin.size))
                return Fail("guest flow entry changed before publication",pin.address);
            DWORD previous{};
            if(!VirtualProtect(reinterpret_cast<void*>(pin.address),1,PAGE_EXECUTE_READWRITE,&previous))
                return Fail("guest flow entry protection publication failed",pin.address);
            const BYTE breakpoint=0xCC;SIZE_T wrote{};
            const bool written=WriteProcessMemory(GetCurrentProcess(),reinterpret_cast<void*>(pin.address),&breakpoint,1,&wrote) && wrote==1;
            BYTE actual{};const bool read=Read(pin.address,&actual,1);
            installed_[i]=read && actual==breakpoint;
            const bool flushed=FlushInstructionCache(GetCurrentProcess(),reinterpret_cast<const void*>(pin.address),1)!=FALSE;
            DWORD unused{};const bool restored=VirtualProtect(reinterpret_cast<void*>(pin.address),1,previous,&unused)!=FALSE;
            // Any partial publication is terminal to the owner; do not retry,
            // rearm, or claim rollback after a failed memory operation.
            if(!written || !installed_[i] || !flushed || !restored)
                return Fail("guest flow partial entry publication requires terminal owned cleanup",pin.address);
        }
        OutputDebugStringA("WinxCoop guest flow installed rareEntries=3 permanentUntilGameExit=true");
        return true;
    }
    bool GuestFlowGate::Handles(const std::uint32_t address) const noexcept
    {
        for(unsigned i=0;i<3;++i)if(installed_[i] && Pins[i].address==address)return true;
        return false;
    }
    bool GuestFlowGate::Lookup(const std::uint32_t target, Record& record) const noexcept
    {
        std::uint32_t id{};
        // Native registry has81 entries, indices0..80. Target81 is the POP
        // sentinel; it must never be read as registry[81].
        return target<=80 && flow_ && Word(flow_+0x18+target*4,record.object) &&
            record.object>=0x10000 && !(record.object&3) && Word(record.object,record.table) &&
            record.table>=0x400000 && record.table<0x740000 &&
            Word(record.object+0x10,id) && id==target;
    }
    bool GuestFlowGate::UserInterface(const Record& record) const noexcept
    {
        std::uint32_t expected{},actual{};
        if(record.table==PauseTable)expected=0x5F0080;
        else if(record.table==MainMenuTable)expected=0x5EF690;
        else if(record.table==DiaryTable)expected=0x5D53C0;
        else if(record.table==OptionTable)
        {
            std::uint32_t id{},identity{};
            // Lookup has already established the registry object. Repeat its
            // exact native state ID here for retained-stack callers too.
            if(!Word(record.object+0x10,id) || id!=66 ||
                !Word(OptionRecord,identity) || identity!=OptionClass)return false;
            expected=OptionGetter;
        }
        return expected && Word(record.table+0x10,actual) && actual==expected;
    }
    bool GuestFlowGate::RetainedWithUiAbove(const Record& record) const noexcept
    {
        std::uint32_t depth{};if(!Word(flow_+0x1AC,depth) || depth>20)return false;
        unsigned matches=0;bool above=false;
        for(std::uint32_t i=0;i<=depth;++i)
        {
            Record candidate{};std::uint32_t id{};
            if(!Word(flow_+0x15C+i*4,candidate.object) || candidate.object<0x10000 ||
                !Word(candidate.object,candidate.table) || !Word(candidate.object+0x10,id))return false;
            if(candidate.object==record.object)
            {
                if(candidate.table!=record.table)return false;
                ++matches;above=true;
            }
            else if(above)
            {
                Record registered{};
                if(!Lookup(id,registered) || registered.object!=candidate.object || !UserInterface(candidate))return false;
            }
        }
        return matches==1;
    }
    bool GuestFlowGate::SafePop() const noexcept
    {
        std::uint32_t depth{},object{},id{};
        Record registered{};
        return Word(flow_+0x1AC,depth) && depth>0 && depth<=20 &&
            Word(flow_+0x15C+depth*4,object) && object>=0x10000 && Word(object+0x10,id) &&
            Lookup(id,registered) && registered.object==object && UserInterface(registered);
    }
    bool GuestFlowGate::ReplayPush(CONTEXT& context, const std::uint32_t address) noexcept
    {
        if(context.Esp<4 || !StackRange(context.Esp-4,4))return Fail("guest flow original PUSH lost its private stack",address);
        SIZE_T wrote{};std::uint32_t after{};
        if(!WriteProcessMemory(GetCurrentProcess(),reinterpret_cast<void*>(context.Esp-4),&context.Esi,4,&wrote) ||
            wrote!=4 || !Word(context.Esp-4,after) || after!=context.Esi)
            return Fail("guest flow original PUSH could not be written and read back",address);
        context.Esp-=4;context.Eip=address+1;return true;
    }
    bool GuestFlowGate::Reject(CONTEXT& context, const std::uint32_t argumentBytes,
        const std::uint32_t target, const std::uint32_t mode, const std::uint32_t delay, const bool playable) noexcept
    {
        std::uint32_t caller{};
        if(!StackRange(context.Esp,4+argumentBytes) || !Word(context.Esp,caller) || caller<0x400000 || caller>=0x740000 ||
            static_cast<std::uint64_t>(context.Esp)+4+argumentBytes>0xFFFFFFFFu)
            return Fail("guest flow rejection lost its original return stack",context.Eip);
        if(playable)++playableBlocked_;else ++unknownBlocked_;
        if(!firstBlockLogged_){firstBlockLogged_=true;TraceBlock(context,caller,target,mode,delay);}
        // These original request functions have opaque EAX results. Known
        // original callers discard that result; rejection returns zero without
        // claiming a successful queue or touching any existing pending slot.
        context.Eax=0;context.Eip=caller;context.Esp+=4+argumentBytes;return true;
    }
    bool GuestFlowGate::Handle(CONTEXT& context) noexcept
    {
        if(failed_ || !Handles(context.Eip))return Fail("guest flow handler lacks its exact installed entry",context.Eip);
        const Pin* pin{};
        for(unsigned i=0;i<3;++i)if(Pins[i].address==context.Eip)pin=&Pins[i];
        BYTE live[32]{};std::uint32_t global{},table{},caller{},target{},argument{};
        if(!pin || !Read(pin->address,live,pin->size) || live[0]!=0xCC || !Same(live+1,pin->bytes+1,pin->size-1) ||
            !Word(FlowGlobal,global) || global!=context.Ecx || global<0x10000 || (global&3) ||
            !Word(global,table) || table!=FlowTable || (flow_ && flow_!=global) ||
            !StackRange(context.Esp,context.Eip==Queue?16:12) || !Word(context.Esp,caller) ||
            !Word(context.Esp+4,target) || !Word(context.Esp+8,argument))
            return Fail("guest flow entry/context/Flow identity differs",context.Eip);
        flow_=global;++calls_;
        if(context.Eip!=Queue)
        {
            ++wrappers_;
            if(target>=1 && target<=50)
            {
                Record assigned{};
                if(context.Eip==Replace && followArmed_)
                {
                    if(retired_ || !ready_ || target!=followTarget_ || argument!=1 ||
                        caller!=followReturn_ || context.Esp!=followEsp_ || followExecuting_ || followCompleted_)
                        return Fail("owned guest follow wrapper identity differs",Replace);
                    followArmed_=false;followExecuting_=true;
                    return ReplayPush(context,Replace);
                }
                if(context.Eip==Push && !retired_ && !ready_ && !initialWrapperSeen_ && !initialQueued_ &&
                    target==assigned_ && argument==0 && caller==0x5CDF91 && Lookup(target,assigned))
                {
                    initialWrapperSeen_=true;initialInProgress_=true;
                    return ReplayPush(context,Push);
                }
                // Reject the entire replace wrapper before it can queue its
                // preliminary POP81(mode1,countdown0) at return595468.
                return Reject(context,8,target,0xFFFFFFFFu,argument,true);
            }
            Record record{};
            if(context.Eip==Push && target==54 && !ready_ && !retired_ && !initialWrapperSeen_ && !logoQueued_ &&
                !logoInProgress_ && argument==1 && caller==0x40E5FA && Lookup(target,record) && record.table==LogoTable)
            {
                std::uint32_t getter{};
                if(!Word(LogoTable+0x10,getter) || getter!=0x5CDD70)return Fail("guest Logo registry type getter differs",LogoTable);
                logoInProgress_=true;return ReplayPush(context,Push);
            }
            if(Lookup(target,record) && UserInterface(record))return ReplayPush(context,context.Eip);
            return Reject(context,8,target,0xFFFFFFFFu,argument,false);
        }

        std::uint32_t delay{};
        if(!Word(context.Esp+12,delay))return Fail("guest flow queue countdown is unreadable",Queue);
        const auto mode=argument;bool allow=false;
        Record record{};
        if(followExecuting_)
        {
            if(target==81 && mode==1 && delay==0 && caller==0x595468 &&
                !followPopQueued_ && !followPrepareQueued_ && !followPushQueued_)
            {followPopQueued_=true;allow=true;}
            else if(target==followTarget_ && mode==4 && delay==0 && caller==0x595495 &&
                followPopQueued_ && !followPrepareQueued_ && !followPushQueued_ && Lookup(target,record))
            {followPrepareQueued_=true;allow=true;}
            else if(target==followTarget_ && mode==0 && delay==2 && caller==0x5954A1 &&
                followPopQueued_ && !followPushQueued_ && Lookup(target,record))
            {followPushQueued_=true;allow=true;}
            if(!allow)return Fail("owned guest follow emitted an unexpected native queue tuple",Queue);
        }
        else if(target>=1 && target<=50)
        {
            if(!retired_ && !ready_ && initialInProgress_ && !initialQueued_ && target==assigned_ &&
                caller==0x595426 && mode==0 && delay==2 && Lookup(target,record))
            {
                initialQueued_=true;initialInProgress_=false;++bootAllowed_;allow=true;
            }
            else if(ready_ && !retired_ && target==assigned_ && mode==3 && delay==1 && caller==0x5954CE &&
                Lookup(target,record) && record.object==playing_ && RetainedWithUiAbove(record))
            {++resumesAllowed_;allow=true;}
            if(!allow)return Reject(context,12,target,mode,delay,true);
        }
        else if(target==81)
        {
            // No registry lookup for this sentinel. In particular, the POP
            // from595450(playable) is not admitted through return595468.
            allow=caller==0x59544B && mode==1 && delay==1 && SafePop();
            if(allow)++popsAllowed_;
        }
        else if(target==54 && !ready_ && !retired_ && logoInProgress_ && !logoQueued_ &&
            !initialWrapperSeen_ && caller==0x595437 && mode==0 && delay==1 && Lookup(target,record) && record.table==LogoTable)
        {logoQueued_=true;logoInProgress_=false;++uiAllowed_;allow=true;}
        else if(Lookup(target,record) && UserInterface(record) && delay==1)
        {
            allow=(mode==0 && caller==0x595437) || (mode==2 && caller==0x5954B0) ||
                (mode==3 && caller==0x5954CE && RetainedWithUiAbove(record));
            if(allow)++uiAllowed_;
        }
        if(!allow)return Reject(context,12,target,mode,delay,false);
        // Replay only the patched original XOR EAX,EAX. LEA EDX,[ECX+1B8]
        // remains native at594E82. XOR's AF is architecturally undefined; use
        // the normal x86 zero choice and preserve every unrelated EFLAGS bit.
        context.Eax=0;context.EFlags=(context.EFlags&~0x8D5u)|0x44u;context.Eip=Queue+2;
        return true;
    }
    bool GuestFlowGate::CanFollow(const std::uint32_t target) const noexcept
    {
        if(failed_ || !prepared_ || !installAttempted_ || !ready_ || retired_ || !playing_ ||
            followTarget_ || target<1 || target>50)return false;
        std::uint32_t global{},depth{},current{},top{},queued{};
        Record old{},next{};
        if(!Word(FlowGlobal,global) || global!=flow_ || !Lookup(assigned_,old) || old.object!=playing_ || old.table!=0x710874 ||
            !Lookup(target,next) || next.table!=0x710874 || !Word(flow_+0x1AC,depth) || depth>20 ||
            !Word(flow_+0x1B0,current) || current!=assigned_ || !Word(flow_+0x15C+depth*4,top) || top!=playing_)
            return false;
        for(unsigned i=0;i<10;++i)if(!Word(flow_+0x1B8+i*12,queued) || queued)return false;
        return true;
    }
    bool GuestFlowGate::PrepareFollow(const std::uint32_t target,const std::uint32_t flow,
        const std::uint32_t privateReturn,const std::uint32_t expectedEntryEsp) noexcept
    {
        BYTE trap{};MEMORY_BASIC_INFORMATION region{};
        if(!CanFollow(target) || flow!=flow_ || !StackRange(expectedEntryEsp,12) ||
            !Read(privateReturn,&trap,1) || trap!=0xCC ||
            !VirtualQuery(reinterpret_cast<void*>(privateReturn),&region,sizeof(region)) || region.State!=MEM_COMMIT ||
            region.Type!=MEM_PRIVATE || (region.Protect&0xFF)!=PAGE_EXECUTE_READ || (region.Protect&PAGE_GUARD))
            return Fail("guest follow permit lacks exact settled Playing/private call ownership",Replace);
        followTarget_=target;followReturn_=privateReturn;followEsp_=expectedEntryEsp;
        followArmed_=true;followExecuting_=followPopQueued_=followPrepareQueued_=followPushQueued_=followCompleted_=false;
        return true;
    }
    bool GuestFlowGate::CompleteFollowRequest(const std::uint32_t target) noexcept
    {
        if(failed_ || !followExecuting_ || followArmed_ || followCompleted_ || target!=followTarget_ ||
            !followPopQueued_ || !followPushQueued_ || retired_)
            return Fail("guest follow completion lacks its single native wrapper/queue receipt",Replace);
        const unsigned count=followPrepareQueued_?3:2;
        for(unsigned i=0;i<10;++i)
        {
            std::uint32_t queued{},mode{},delay{};
            if(!Word(flow_+0x1B8+i*12,queued))return Fail("guest follow result queue is unreadable",Queue);
            if(i>=count){if(queued)return Fail("guest follow wrote an extra queue target",Queue);continue;}
            if(!Word(flow_+0x1BC+i*12,mode) || !Word(flow_+0x1C0+i*12,delay) ||
                (i==0?(queued!=81 || mode!=1 || delay!=0):
                    (i==count-1?(queued!=target || mode!=0 || delay!=2):(queued!=target || mode!=4 || delay!=0))))
                return Fail("guest follow native queue result differs from the pinned call",Queue);
        }
        followExecuting_=false;followCompleted_=true;
        OutputDebugStringA("WinxCoop guest follow request admitted: original POP/optional prepare/PUSH queued; entry not yet complete");
        return true;
    }
    bool GuestFlowGate::BeginNextScene(const std::uint32_t target) noexcept
    {
        Record record{};std::uint32_t global{},table{},depth{},top{},current{},pending{};
        if(failed_ || !retired_ || !followCompleted_ || followExecuting_ || followArmed_ || target!=followTarget_ ||
            !Word(FlowGlobal,global) || global!=flow_ || !Word(flow_,table) || table!=FlowTable ||
            !Lookup(target,record) || record.table!=0x710874 ||
            !Word(flow_+0x1AC,depth) || depth>20 || !Word(flow_+0x15C+depth*4,top) || top!=record.object ||
            !Word(flow_+0x1B0,current) || current!=target || !Word(flow_+0x1B8,pending) || pending)
            return Fail("guest next scene lacks actual retirement and assigned fresh Playing",Queue);
        for(unsigned i=1;i<10;++i)if(!Word(flow_+0x1B8+i*12,pending) || pending)
            return Fail("guest next scene still has a later queued native operation",Queue);
        assigned_=target;playing_=0;ready_=retired_=false;
        // Native595450 has already queued this push. Do not reopen startup,
        // Logo or ordinary unowned playable requests and do not republish hooks.
        initialWrapperSeen_=initialQueued_=true;initialInProgress_=false;
        followTarget_=followReturn_=followEsp_=0;
        followCompleted_=followExecuting_=followArmed_=followPopQueued_=followPrepareQueued_=followPushQueued_=false;
        return true;
    }
    bool GuestFlowGate::SceneReady() noexcept
    {
        if(failed_)return false;
        if(!prepared_ || ready_ || retired_)return true;
        Record record{};std::uint32_t depth{},top{},current{},pending{};
        if(!initialQueued_ || !Lookup(assigned_,record) || !Word(flow_+0x1AC,depth) || depth>20 ||
            !Word(flow_+0x15C+depth*4,top) || top!=record.object || !Word(flow_+0x1B0,current) || current!=assigned_ ||
            !Word(flow_+0x1B8,pending) || pending)
            return Fail("guest flow SceneReady lacks its admitted actual assigned scene");
        playing_=record.object;ready_=true;initialInProgress_=false;
        return true;
    }
    void GuestFlowGate::Retire() noexcept
    {
        if(!prepared_ || retired_)return;
        retired_=true;initialInProgress_=false;logoInProgress_=false;TraceCounters();
    }
    void GuestFlowGate::TraceCounters() const noexcept
    {
        if(!prepared_ && !failed_)return;
        char text[480]{};char* cursor=text;const char* end=text+sizeof(text)-1;
        Append(cursor,end,"WinxCoop guest flow counters");
        Hex(cursor,end," calls=",calls_);Hex(cursor,end," wrappers=",wrappers_);
        Hex(cursor,end," bootAllowed=",bootAllowed_);Hex(cursor,end," uiAllowed=",uiAllowed_);
        Hex(cursor,end," popsAllowed=",popsAllowed_);Hex(cursor,end," resumesAllowed=",resumesAllowed_);
        Hex(cursor,end," playableBlocked=",playableBlocked_);Hex(cursor,end," unknownBlocked=",unknownBlocked_);
        Hex(cursor,end," ready=",ready_?1:0);Hex(cursor,end," retired=",retired_?1:0);
        Hex(cursor,end," failedAt=",failedAddress_);Append(cursor,end," failure=");Append(cursor,end,failure_);
        OutputDebugStringA(text);
    }
}
