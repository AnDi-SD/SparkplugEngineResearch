// Own passive-guest policy. Native hot paths use bounded integer-only stubs;
// the debugger is not entered once per entity notification.
#include "GuestIsolation.h"
#include "GuestIsolationCatalog.h"
#include "GuestInputCatalog.h"
#include "GuestFlowGate.h"
#include "PinnedImage.h"
#include <new>

namespace winxcoop::adapter
{
    namespace
    {
        using U = std::uint32_t;
        using namespace guest_isolation;
        constexpr U CodeBytes=8192, MaxSites=32, Dropped=60, Kept=61, Other=62, Foreign=63;
        constexpr U MotionTables[]={0x6F6438,0x6FA288,0x6F64F8,0x6F670C};
        constexpr U MotionIds[]={0x0003CC73,0x37810000,0x680838E4,0xCD2B2876};
        constexpr U UiTables[]={0x6F522C,0x6FE988,0x6FEFAC,0x6FF058,0x6FF1F0};
        constexpr U UiIds[]={0x7FC114C3,0x69F878BA,0x7F2E3F1A,0x401A393C,0x65F4C9A8};
        constexpr U AlphaGlobal=0x75528C, AlphaTable=0x702B64, AlphaClass=0x41F81774;
        constexpr U TransparentFade=0x593A60, CinematicStart=0x4EA120;
        struct PresentationPin final {U address,size;BYTE bytes[32];};
        // Complete instruction spans of the original transparent fade and its
        // null-recipient notification path. They are call admission, not code
        // patches or a whole-image hash. The fade is our explicit presentation
        // completion after withholding the client's cinematic start.
        constexpr PresentationPin PresentationPins[]={
            {0x593A60,16,{0x51,0xD9,0x05,0x34,0x2B,0x70,0x00,0x56,0x8B,0xF1,0xD9,0x86,0x4C,0x03,0x00,0x00}},
            {0x593A83,12,{0x8B,0x44,0x24,0x04,0x50,0x8B,0xCE,0xE8,0xB1,0xFE,0xFF,0xFF}},
            {0x593A8F,14,{0xC6,0x86,0x44,0x03,0x00,0x00,0x01,0xC6,0x86,0x45,0x03,0x00,0x00,0x00}},
            {0x593A9D,20,{0xC7,0x86,0x48,0x03,0x00,0x00,0x00,0x00,0x80,0x3F,0xC7,0x86,0x4C,0x03,0x00,0x00,0x00,0x00,0x00,0x00}},
            {0x593ACE,13,{0xA1,0x70,0xF6,0x75,0x00,0x33,0xD2,0xF7,0x35,0x50,0xE0,0x74,0x00}},
            {0x593ADB,31,{0x8B,0x4C,0x24,0x0C,0x8B,0x54,0x24,0x10,0x89,0x8E,0x54,0x03,0x00,0x00,
                0x89,0x96,0x60,0x03,0x00,0x00,0x89,0x86,0x50,0x03,0x00,0x00,0x5E,0x59,0xC2,0x08,0x00}},
            {0x593940,18,{0x83,0xEC,0x20,0x56,0x8B,0xF1,0x8B,0x8E,0x60,0x03,0x00,0x00,0x33,0xC0,0x3B,0xC8,0x74,0x40}},
            {0x593992,14,{0xC6,0x86,0x45,0x03,0x00,0x00,0x01,0x5E,0x83,0xC4,0x20,0xC2,0x04,0x00}},
        };
        struct PresentationFade final
        {
            BYTE enabled{},notified{};
            U from{},to{},duration{},material{},recipient{};
        };
        enum class InputDeviceRegister { Esi, Ebp };
        bool Read(U at,void* out,SIZE_T bytes) noexcept
        {SIZE_T got{};return at>=0x10000 && ReadProcessMemory(GetCurrentProcess(),reinterpret_cast<const void*>(at),out,bytes,&got) && got==bytes;}
        U Word(U at) noexcept {U value{};Read(at,&value,4);return value;}
        bool AlphaStorage(U alpha) noexcept
        {
            MEMORY_BASIC_INFORMATION region{};
            return alpha>=0x10000 && !(alpha&3) && alpha<=0xFFFFFC90u &&
                VirtualQuery(reinterpret_cast<const void*>(alpha),&region,sizeof(region))==sizeof(region) &&
                region.State==MEM_COMMIT && region.Type==MEM_PRIVATE && !(region.Protect&PAGE_GUARD) &&
                (region.Protect&0xFF)==PAGE_READWRITE &&
                static_cast<std::uint64_t>(alpha)+0x370<=reinterpret_cast<U>(region.BaseAddress)+region.RegionSize;
        }
        bool ReadPresentation(U alpha,PresentationFade& fade) noexcept
        {
            BYTE data[32]{};
            if(!Read(alpha+0x344,data,sizeof(data)))return false;
            fade.enabled=data[0];fade.notified=data[1];
            CopyMemory(&fade.from,data+4,4);CopyMemory(&fade.to,data+8,4);
            CopyMemory(&fade.duration,data+0x10,4);CopyMemory(&fade.material,data+0x14,4);
            CopyMemory(&fade.recipient,data+0x1C,4);return true;
        }
        bool TerminalOpaque(const PresentationFade& fade) noexcept
        {
            return fade.enabled==1 && fade.notified<=1 && fade.from==0 && fade.to==0x3F800000 &&
                !fade.duration && fade.material>=0x10000 && !(fade.material&3) && !fade.recipient;
        }
        bool FadeTransparent(U alpha) noexcept
        {
            // Existing Tick VEH owns FP/SSE restoration. This is the original
            // void thiscall(duration,recipient), whose callee performs RET8.
            __try {reinterpret_cast<void(__thiscall*)(void*,U,void*)>(TransparentFade)(reinterpret_cast<void*>(alpha),0,nullptr);}
            __except(EXCEPTION_EXECUTE_HANDLER){return false;}
            return true;
        }
        void TracePresentation(const char* outcome,U blocked,U alpha,bool called,
            const PresentationFade* before=nullptr,const PresentationFade* after=nullptr) noexcept
        {
            char text[640]{};char* p=text;const char* end=text+sizeof(text)-1;
            const auto add=[&](const char* value) noexcept {while(*value && p<end)*p++=*value++;};
            const auto word=[&](const char* label,U value) noexcept
            {add(label);for(unsigned i=0;i<8 && p<end;++i)*p++="0123456789ABCDEF"[(value>>(28-i*4))&15];};
            add("WinxCoop guest presentation outcome=");add(outcome);word(" cineBlocked=",blocked);
            word(" alpha=",alpha);word(" nativeCall=",called?1:0);
            if(before)
            {
                word(" beforeEnabled=",before->enabled);word(" beforeNotified=",before->notified);
                word(" beforeFrom=",before->from);word(" beforeTo=",before->to);
                word(" beforeDuration=",before->duration);word(" beforeRecipient=",before->recipient);
            }
            if(after)
            {
                word(" afterEnabled=",after->enabled);word(" afterNotified=",after->notified);
                word(" afterFrom=",after->from);word(" afterTo=",after->to);
                word(" afterDuration=",after->duration);word(" afterRecipient=",after->recipient);
            }
            add(" oneShot=true directAlphaWrites=false");OutputDebugStringA(text);
        }
        bool Equal(const void* a,const void* b,SIZE_T bytes) noexcept
        {const auto* x=static_cast<const BYTE*>(a);const auto* y=static_cast<const BYTE*>(b);for(SIZE_T i=0;i<bytes;++i)if(x[i]!=y[i])return false;return true;}
        bool PrivateWrite(U at,const void* data,SIZE_T bytes) noexcept
        {
            MEMORY_BASIC_INFORMATION region{};
            if(!bytes || bytes>32 || VirtualQuery(reinterpret_cast<const void*>(at),&region,sizeof(region))!=sizeof(region) ||
                region.State!=MEM_COMMIT || region.Type!=MEM_PRIVATE || (region.Protect&PAGE_GUARD) ||
                (region.Protect&0xFF)!=PAGE_READWRITE || at<reinterpret_cast<U>(region.BaseAddress) ||
                static_cast<std::uint64_t>(at)+bytes>reinterpret_cast<U>(region.BaseAddress)+region.RegionSize)return false;
            SIZE_T wrote{};BYTE after[32]{};
            return WriteProcessMemory(GetCurrentProcess(),reinterpret_cast<void*>(at),data,bytes,&wrote) && wrote==bytes &&
                Read(at,after,bytes) && Equal(after,data,bytes);
        }
        bool CodeWrite(U at,const BYTE* data,U bytes) noexcept
        {
            DWORD old{},ignored{};SIZE_T wrote{};BYTE after[32]{};
            if(!bytes || bytes>32 || !VirtualProtect(reinterpret_cast<void*>(at),bytes,PAGE_EXECUTE_READWRITE,&old))return false;
            const bool written=WriteProcessMemory(GetCurrentProcess(),reinterpret_cast<void*>(at),data,bytes,&wrote)!=FALSE && wrote==bytes;
            const bool flushed=FlushInstructionCache(GetCurrentProcess(),reinterpret_cast<void*>(at),bytes)!=FALSE;
            const bool restored=VirtualProtect(reinterpret_cast<void*>(at),bytes,old,&ignored)!=FALSE;
            return written && flushed && restored && Read(at,after,bytes) && Equal(after,data,bytes);
        }
        [[noreturn]] void __cdecl ForeignThreadFailure() noexcept
        {
            OutputDebugStringA("WinxCoop guest isolation: foreign-thread effect source rejected");
            TerminateProcess(GetCurrentProcess(),126);
            __assume(0);
        }
        struct Emitter final
        {
            BYTE* data{};U base{},used{};bool good{true};
            U Address() const noexcept {return base+used;}
            void Byte(BYTE v) noexcept {if(used>=CodeBytes){good=false;return;}data[used++]=v;}
            void Word32(U v) noexcept {for(unsigned i=0;i<4;++i)Byte(static_cast<BYTE>(v>>(8*i)));}
            void Bytes(const BYTE* p,U count) noexcept {for(U i=0;i<count;++i)Byte(p[i]);}
            U Branch(BYTE condition=0) noexcept
            {if(condition){Byte(0x0F);Byte(condition);}else Byte(0xE9);const U at=used;Word32(0);return at;}
            void Resolve(U offset,U destination) noexcept
            {if(offset+4>used){good=false;return;}const U delta=destination-(base+offset+4);for(unsigned i=0;i<4;++i)data[offset+i]=static_cast<BYTE>(delta>>(8*i));}
            void Jump(U destination) noexcept {const U at=Branch();Resolve(at,destination);}
            void Increment(volatile LONG* counter) noexcept {Byte(0xF0);Byte(0xFF);Byte(0x05);Word32(reinterpret_cast<U>(counter));}
            void Protect(DWORD thread) noexcept
            {
                Byte(0x9C);Byte(0x50); // PUSHFD; PUSH EAX. All stubs restore both.
                Byte(0x64);Byte(0xA1);Word32(0x24);Byte(0x3D);Word32(thread);
                const U same=Branch(0x84);Byte(0xB8);Word32(reinterpret_cast<U>(&ForeignThreadFailure));Byte(0xFF);Byte(0xD0);
                Byte(0x0F);Byte(0x0B);Resolve(same,Address());
            }
            void Unprotect() noexcept {Byte(0x58);Byte(0x9D);}
        };
        bool GameplayContext() noexcept
        {
            const U flow=Word(0x755294),input=Word(0x7552A0),device=Word(input+0x1C),depth=Word(flow+0x1AC);
            if(Word(flow)!=0x702D2C || depth>20 || Word(flow+0x1B8) || Word(input)!=0x6F5508 ||
                Word(device)!=0x7116CC || Word(device+0x14)!=3)return false;
            const U playing=Word(flow+0x15C+depth*4),level=Word(flow+0x1B0);
            return Word(playing)==0x710874 && level>=1 && level<=50 && Word(playing+0x10)==level;
        }
    }
    struct GuestIsolation::State final
    {
        struct Patch final {Site site{};BYTE replacement[32]{};bool input{},installed{};};
        Patch patches[MaxSites]{};U count{},inputTailIndex{},cinematicStartIndex{MaxSites},assigned{};
        DWORD thread{};BYTE* code{};BYTE emitted[CodeBytes]{};U emittedBytes{};PinnedImage* image{};
        volatile LONG counters[64]{};U motion[4]{},record{};
        LONG presentationCinematicBase{},followPresentationBase{};
        bool prepared{},published{},installed{},bound{},retired{},sceneReady{},presentationComplete{},presentationCalled{};
        GuestFlowGate flow;

        bool Add(const Site& site,bool input) noexcept
        {
            if(count==MaxSites || !site.size || site.size>32 ||
                (site.action!=Action::InputTail && site.size<5))return false;
            for(U i=0;i<count;++i)if(site.address<patches[i].site.address+patches[i].site.size &&
                patches[i].site.address<site.address+site.size)return false;
            patches[count].site=site;patches[count].input=input;
            if(site.action==Action::InputTail)inputTailIndex=count;
            if(site.address==CinematicStart)cinematicStartIndex=count;
            ++count;return true;
        }
        void Typed(Emitter& e,U index) noexcept
        {
            const auto& site=patches[index].site;e.Protect(thread);e.Increment(&counters[index]);
            e.Byte(0x83);e.Byte(0x3A);e.Byte(0x1E);const U other=e.Branch(0x85);
            U passes[9]{};unsigned n=0;
            for(U i=0;i<4;++i){e.Byte(0x3B);e.Byte(0x0D);e.Word32(reinterpret_cast<U>(motion+i));passes[n++]=e.Branch(0x84);}
            e.Byte(0x8B);e.Byte(0x01);
            for(U table:UiTables){e.Byte(0x3D);e.Word32(table);passes[n++]=e.Branch(0x84);}
            e.Increment(&counters[Dropped]);e.Unprotect();e.Jump(site.next);
            for(U i=0;i<n;++i)e.Resolve(passes[i],e.Address());
            e.Increment(&counters[Kept]);const U forward=e.Branch();
            e.Resolve(other,e.Address());e.Increment(&counters[Other]);e.Resolve(forward,e.Address());e.Unprotect();
            // Preserve the literal original return address and argument stack.
            e.Byte(0x8B);e.Byte(0x01);e.Byte(0x52);e.Byte(0x68);e.Word32(site.next);e.Byte(0xFF);e.Byte(0x60);e.Byte(0x04);
        }
        void InputSource(Emitter& e,U index,InputDeviceRegister deviceRegister) noexcept
        {
            const auto& site=patches[index].site;e.Protect(thread);e.Byte(0x52); // retain EDX for the context check
            U passes[20]{};unsigned n=0;
            // Ordinary producers carry ESI=device; the keyboard tail instead
            // carries EBP=device and ESI=command. Neither register is modified.
            // EBP requires a zero disp8 to encode [EBP], not the absolute r/m5.
            const bool ebp=deviceRegister==InputDeviceRegister::Ebp;
            e.Byte(0x81);e.Byte(ebp?0x7D:0x3E);if(ebp)e.Byte(0);e.Word32(0x7116CC);passes[n++]=e.Branch(0x85);
            e.Byte(0x83);e.Byte(ebp?0x7D:0x7E);e.Byte(0x14);e.Byte(3);passes[n++]=e.Branch(0x85);
            e.Byte(0xA1);e.Word32(0x755294);e.Byte(0x85);e.Byte(0xC0);passes[n++]=e.Branch(0x84);
            e.Byte(0x81);e.Byte(0x38);e.Word32(0x702D2C);passes[n++]=e.Branch(0x85);
            e.Byte(0x83);e.Byte(0xB8);e.Word32(0x1B8);e.Byte(0);passes[n++]=e.Branch(0x85);
            e.Byte(0x8B);e.Byte(0x90);e.Word32(0x1AC);e.Byte(0x83);e.Byte(0xFA);e.Byte(20);passes[n++]=e.Branch(0x87);
            e.Byte(0x8B);e.Byte(0x84);e.Byte(0x90);e.Word32(0x15C);e.Byte(0x85);e.Byte(0xC0);passes[n++]=e.Branch(0x84);
            e.Byte(0x81);e.Byte(0x38);e.Word32(0x710874);passes[n++]=e.Branch(0x85);
            e.Byte(0x8B);e.Byte(0x50);e.Byte(0x10);e.Byte(0x83);e.Byte(0xFA);e.Byte(1);passes[n++]=e.Branch(0x82);
            e.Byte(0x83);e.Byte(0xFA);e.Byte(50);passes[n++]=e.Branch(0x87);
            e.Byte(0xA1);e.Word32(0x755294);e.Byte(0x3B);e.Byte(0x90);e.Word32(0x1B0);passes[n++]=e.Branch(0x85);
            e.Byte(0xA1);e.Word32(0x7552A0);e.Byte(0x85);e.Byte(0xC0);passes[n++]=e.Branch(0x84);
            e.Byte(0x81);e.Byte(0x38);e.Word32(0x6F5508);passes[n++]=e.Branch(0x85);
            e.Byte(0x3B);e.Byte(ebp?0x68:0x70);e.Byte(0x1C);passes[n++]=e.Branch(0x85);
            e.Increment(&counters[index]);e.Byte(0x5A);e.Unprotect();e.Jump(site.next);
            for(U i=0;i<n;++i)e.Resolve(passes[i],e.Address());e.Byte(0x5A);e.Unprotect();
            // These catalogued complete prefixes contain no relative branch.
            e.Bytes(site.bytes.data(),site.size);e.Jump(site.address+site.size);
        }
        void ReturnFalse(Emitter& e,U index) noexcept
        {
            const auto& site=patches[index].site;e.Protect(thread);
            U passes[2]{};
            if(site.address==0x4E4560)
            {
                e.Byte(0x8B);e.Byte(0x44);e.Byte(0x24);e.Byte(0x0C);e.Byte(0x81);e.Byte(0x38);e.Word32(0x27DB);
                const U blocked=e.Branch(0x84);e.Byte(0x81);e.Byte(0x38);e.Word32(0x27EA);passes[0]=e.Branch(0x85);
                e.Resolve(blocked,e.Address());
            }
            e.Increment(&counters[index]);e.Unprotect();e.Byte(0x33);e.Byte(0xC0);
            if(site.stackBytes){e.Byte(0xC2);e.Byte(static_cast<BYTE>(site.stackBytes));e.Byte(0);}else e.Byte(0xC3);
            if(site.address==0x4E4560)
            {e.Resolve(passes[0],e.Address());e.Unprotect();e.Bytes(site.bytes.data(),site.size);e.Jump(site.next);}
        }
        bool Emit() noexcept
        {
            Emitter e{emitted,reinterpret_cast<U>(code)};
            for(U i=0;i<count;++i)
            {
                auto& patch=patches[i];const auto& site=patch.site;
                if(site.action==Action::InputTail){patch.replacement[0]=0xCC;continue;}
                const U target=e.Address();
                if(site.action==Action::TypedTick)Typed(e,i);
                else if(site.action==Action::ReturnFalse)ReturnFalse(e,i);
                else if(site.action==Action::Skip)
                {
                    const bool cheat=site.address==0x60437B || site.address==0x6044AE;
                    if(patch.input && !cheat)InputSource(e,i,site.address==0x604ABC?InputDeviceRegister::Ebp:InputDeviceRegister::Esi);
                    else {e.Protect(thread);e.Increment(&counters[i]);e.Unprotect();e.Jump(site.next);}
                }
                else return false;
                for(U n=0;n<site.size;++n)patch.replacement[n]=0x90;
                patch.replacement[0]=0xE9;const U relative=target-(site.address+5);CopyMemory(patch.replacement+1,&relative,4);
            }
            emittedBytes=e.used;return e.good && emittedBytes && emittedBytes<=CodeBytes;
        }
        void Trace() const noexcept
        {
            char text[384]{};char* p=text;const auto add=[&p](const char* s) noexcept {while(*s)*p++=*s++;};
            const auto word=[&](const char* label,U v) noexcept
            {add(label);for(unsigned i=0;i<8;++i)*p++="0123456789ABCDEF"[(v>>(28-i*4))&15];};
            add("WinxCoop guest isolation counters");word(" typed=",counters[0]);word(" dropped=",counters[Dropped]);
            word(" kept=",counters[Kept]);word(" other=",counters[Other]);word(" input=",counters[inputTailIndex]);
            word(" save=",counters[1]+counters[2]+counters[3]);word(" damage=",counters[4]+counters[5]);word(" bound=",bound?1:0);
            word(" cine=",cinematicStartIndex<MaxSites?counters[cinematicStartIndex]:0);
            word(" presentationDone=",presentationComplete?1:0);word(" presentationCalled=",presentationCalled?1:0);
            OutputDebugStringA(text);flow.TraceCounters();
        }
    };
    bool GuestIsolation::Fail(const char* reason) noexcept {failure_=reason;return false;}
    bool GuestIsolation::Prepare(PinnedImage& image,DWORD thread,U level) noexcept
    {
        if(state_ || !thread || level<1 || level>50)return Fail("invalid or repeated guest isolation preparation");
        void* storage=HeapAlloc(GetProcessHeap(),0,sizeof(State));if(!storage)return Fail("guest isolation scratch allocation failed");
        state_=::new(storage) State;auto& s=*state_;s.thread=thread;s.image=&image;s.assigned=level;
        for(const auto& site:guest_isolation::Sites)if(!s.Add(site,false))return Fail("guest effect site catalogue exceeds bounds");
        for(const auto& site:guest_isolation::InputSites)if(!s.Add(site,true))return Fail("guest input site catalogue exceeds bounds");
        for(U i=0;i<s.count;++i)if(!image.VerifyFeatureCode(s.patches[i].site.address,s.patches[i].site.bytes.data(),s.patches[i].site.size))
            return Fail(s.patches[i].site.name);
        for(const auto& pin:guest_isolation::InputDestinations)
            if(!image.VerifyFeatureCode(pin.address,pin.bytes.data(),pin.size))return Fail(pin.name);
        if(s.cinematicStartIndex==MaxSites)return Fail("guest presentation has no cinematic start counter");
        for(const auto& pin:PresentationPins)
            if(!image.VerifyFeatureCode(pin.address,pin.bytes,pin.size))return Fail("guest transparent presentation code differs");
        if(!image.VerifyFeatureType(AlphaTable,AlphaClass,true))return Fail("guest presentation Alpha class identity differs");
        for(U i=0;i<5;++i)if(!image.VerifyFeatureType(UiTables[i],UiIds[i],true))return Fail("guest presentation receiver identity differs");
        for(U i=0;i<4;++i)if(!image.VerifyFeatureType(MotionTables[i],MotionIds[i],true))return Fail("guest motion receiver identity differs");
        if(!s.flow.Prepare(image,level))return Fail(s.flow.Failure());
        s.code=static_cast<BYTE*>(VirtualAlloc(nullptr,CodeBytes,MEM_RESERVE|MEM_COMMIT,PAGE_READWRITE));
        if(!s.code || !s.Emit())return Fail("guest native filter emission failed");
        CopyMemory(s.code,s.emitted,s.emittedBytes);DWORD old{};
        if(!Equal(s.code,s.emitted,s.emittedBytes) || !VirtualProtect(s.code,CodeBytes,PAGE_EXECUTE_READ,&old) || old!=PAGE_READWRITE ||
            !FlushInstructionCache(GetCurrentProcess(),s.code,s.emittedBytes))return Fail("guest native filter RX seal/readback failed");
        s.prepared=true;return true;
    }
    bool GuestIsolation::Install() noexcept
    {
        if(!state_ || !state_->prepared || state_->published || GetCurrentThreadId()!=state_->thread)return Fail("guest policy publication has no prepared main-thread owner");
        auto& s=*state_;s.published=true; // any later failure is terminal, never free reachable code
        for(U i=0;i<s.count;++i)
        {
            auto& p=s.patches[i];BYTE before[32]{};
            if(!Read(p.site.address,before,p.site.size) || !Equal(before,p.site.bytes.data(),p.site.size) ||
                !CodeWrite(p.site.address,p.replacement,p.site.size))return Fail(p.site.name);
            p.installed=true;
        }
        if(!s.flow.Install())return Fail(s.flow.Failure());s.installed=true;
        OutputDebugStringA("WinxCoop guest isolation installed: native effect filters; stock physics/profile/animation retained; no per-entity debug trap");
        return true;
    }
    bool GuestIsolation::BindMotion(U record) noexcept
    {
        if(!state_ || !state_->installed || state_->retired || state_->bound || Word(record)!=0x710F80 || Word(record+0x20)!=11)
            return Fail("guest motion binding requires one original eleven-component player record");
        auto& s=*state_;
        for(U i=0;i<5;++i)if(!s.image->VerifyFeatureType(UiTables[i],UiIds[i]))return Fail("initialized guest presentation class identity differs");
        for(U i=0;i<4;++i)if(!s.image->VerifyFeatureType(MotionTables[i],MotionIds[i]))return Fail("initialized guest motion class identity differs");
        const U head=Word(record+0x1C);U link=Word(head),previous=head,seen[11]{},found[4]{};
        for(U i=0;i<11;++i)
        {
            U item[3]{};if(link<0x10000 || link==head || !Read(link,item,sizeof(item)) || item[1]!=previous || item[2]<0x10000)
                return Fail("guest motion component chain is not bounded/reciprocal");
            for(U n=0;n<i;++n)if(seen[n]==link)return Fail("guest motion component chain repeats");seen[i]=link;
            const U table=Word(item[2]);for(U n=0;n<4;++n)if(table==MotionTables[n])
            {if(found[n] || Word(item[2]+0x24)!=record)return Fail("guest motion receiver aliases or has a foreign owner");found[n]=item[2];}
            previous=link;link=item[0];
        }
        if(link!=head || Word(head+4)!=previous)return Fail("guest motion component tail differs");
        for(U n=0;n<4;++n)if(!found[n])return Fail("guest motion recipe lacks an exact local receiver");
        s.record=record;CopyMemory(s.motion,found,sizeof(found));s.bound=true;
        OutputDebugStringA("WinxCoop guest isolation motion bound: exact local Character/FSM/MoveCtrl/Animator; geometry physics retained");
        s.Trace();return true;
    }
    bool GuestIsolation::VerifyMotion(U record) noexcept
    {
        if(!state_ || !state_->bound || state_->retired || state_->record!=record)return Fail("guest motion epoch changed");
        for(U n=0;n<4;++n)
        {BYTE bytes[0x28]{};if(!Read(state_->motion[n],bytes,sizeof(bytes)))return Fail("guest motion receiver is unreadable");
            U table{},owner{};CopyMemory(&table,bytes,4);CopyMemory(&owner,bytes+0x24,4);
            if(table!=MotionTables[n] || owner!=record)return Fail("guest motion receiver identity changed");}
        return true;
    }
    bool GuestIsolation::Handles(U address) const noexcept
    {return state_ && state_->installed && (address==0x4E2D3E || state_->flow.Handles(address));}
    bool GuestIsolation::Handle(CONTEXT& context) noexcept
    {
        if(!state_ || !state_->installed || GetCurrentThreadId()!=state_->thread)return Fail("guest input/flow trap left its original main thread");
        if(state_->flow.Handles(context.Eip))return state_->flow.Handle(context) || Fail(state_->flow.Failure());
        BYTE instruction{};
        if(context.Eip!=0x4E2D3E || !Read(context.Eip,&instruction,1) || instruction!=0xCC)return Fail("guest input return trap identity differs");
        if(GameplayContext())
        {
            const U input=Word(0x7552A0),command=Word(input+0x18);BYTE zero[14]{};
            if(Word(command)!=0x703090 || !PrivateWrite(command+0x1B,zero,1) || !PrivateWrite(command+0x1D,zero,14))
                return Fail("guest command normalization failed its typed private-heap gate");
            InterlockedIncrement(&state_->counters[state_->inputTailIndex]);
        }
        U caller{};if(!Read(context.Esp,&caller,4) || caller<0x401000 || caller>=0x6DA000 || context.Esp>0xFFFFFFFB)
            return Fail("guest input RET has no original bounded caller");
        context.Eip=caller;context.Esp+=4;return true; // exact original C3, all other context unchanged
    }
    bool GuestIsolation::SceneReady() noexcept
    {
        if(!state_ || state_->retired)return Fail("guest presentation readiness has no active state");
        if(!state_->flow.SceneReady())return Fail(state_->flow.Failure());
        state_->sceneReady=true;return true;
    }
    bool GuestIsolation::CanFollow(const U target) const noexcept
    {
        return state_ && state_->installed && state_->bound && state_->sceneReady && !state_->retired &&
            GetCurrentThreadId()==state_->thread && state_->flow.CanFollow(target);
    }
    bool GuestIsolation::PrepareFollow(const U target,const U flow,const U privateReturn,const U expectedEntryEsp) noexcept
    {
        if(!CanFollow(target))return Fail("guest follow permit has no active bound assigned scene/main thread");
        if(!state_->flow.PrepareFollow(target,flow,privateReturn,expectedEntryEsp))return Fail(state_->flow.Failure());
        // Baseline before native Exit/Entry: a new cinematic may already be
        // withheld during LoadLevel, before the next SceneReady observation.
        state_->followPresentationBase=InterlockedCompareExchange(&state_->counters[state_->cinematicStartIndex],0,0);
        return true;
    }
    bool GuestIsolation::CompleteFollowRequest(const U target) noexcept
    {
        if(!state_ || !state_->installed || GetCurrentThreadId()!=state_->thread)
            return Fail("guest follow request completion lost its installed/main-thread owner");
        return state_->flow.CompleteFollowRequest(target) || Fail(state_->flow.Failure());
    }
    bool GuestIsolation::BeginNextScene(const U target) noexcept
    {
        if(!state_ || !state_->installed || !state_->retired || GetCurrentThreadId()!=state_->thread)
            return Fail("guest next scene lacks actual previous retirement/main-thread ownership");
        auto& s=*state_;
        if(!s.flow.BeginNextScene(target))return Fail(s.flow.Failure());
        ZeroMemory(s.motion,sizeof(s.motion));s.record=0;s.assigned=target;
        s.bound=s.retired=s.sceneReady=s.presentationComplete=s.presentationCalled=false;
        s.presentationCinematicBase=s.followPresentationBase;
        OutputDebugStringA("WinxCoop guest isolation next scene admitted: original hooks retained; motion/presentation binding reset");
        return true;
    }
    bool GuestIsolation::CompletePresentation() noexcept
    {
        if(!state_ || !state_->installed || !state_->bound || !state_->sceneReady || state_->retired ||
            GetCurrentThreadId()!=state_->thread)return Fail("guest presentation completion left its admitted scene/main thread");
        auto& s=*state_;
        if(s.presentationComplete)return true;
        const LONG blocked=InterlockedCompareExchange(&s.counters[s.cinematicStartIndex],0,0);
        if(blocked<=s.presentationCinematicBase)return true; // A request for this scene may arrive after first SceneReady.
        s.presentationComplete=true; // One observation/attempt; never repair later fades or retry a native failure.
        const U flow=Word(0x755294);
        if(!GameplayContext() || Word(flow+0x1B0)!=s.assigned)
            return Fail("guest presentation completion lost assigned Playing context");
        U alpha{};PresentationFade before{};
        if(!Read(AlphaGlobal,&alpha,4) || !AlphaStorage(alpha) || Word(alpha)!=AlphaTable || !ReadPresentation(alpha,before))
        {TracePresentation("skip-alpha-not-live",static_cast<U>(blocked),alpha,false);return true;}
        if(!TerminalOpaque(before))
        {TracePresentation("skip-fade-not-terminal-opaque",static_cast<U>(blocked),alpha,false,&before);return true;}
        if(!s.image->VerifyFeatureType(AlphaTable,AlphaClass))return Fail("initialized guest Alpha identity differs");
        for(const auto& pin:PresentationPins)
            if(!s.image->VerifyFeatureCode(pin.address,pin.bytes,pin.size))return Fail("guest transparent presentation changed before call");
        U divisor{};
        if(!Read(0x74E050,&divisor,4) || !divisor)return Fail("guest transparent presentation has no native clock divisor");
        PresentationFade recheck{};
        if(Word(AlphaGlobal)!=alpha || !AlphaStorage(alpha) || Word(alpha)!=AlphaTable || !ReadPresentation(alpha,recheck) ||
            !TerminalOpaque(recheck) || recheck.material!=before.material)
        {TracePresentation("skip-fade-changed-before-call",static_cast<U>(blocked),alpha,false,&before);return true;}
        // In particular, a new null recipient does not cancel an OLD nonnull
        // recipient: original593A60 calls NotifyFade before replacing it.
        s.presentationCalled=true;
        if(!FadeTransparent(alpha))
        {TracePresentation("native-exception-terminal",static_cast<U>(blocked),alpha,true,&before);return Fail("guest transparent presentation native call raised an exception");}
        PresentationFade after{};
        if(Word(AlphaGlobal)!=alpha || Word(alpha)!=AlphaTable || !ReadPresentation(alpha,after) ||
            after.enabled!=1 || after.notified!=0 || after.from!=0x3F800000 || after.to || after.duration || after.recipient ||
            after.material!=before.material)
        {TracePresentation("native-poststate-differs",static_cast<U>(blocked),alpha,true,&before,&after);return Fail("guest transparent presentation native poststate differs");}
        TracePresentation("transparent-requested",static_cast<U>(blocked),alpha,true,&before,&after);
        return true; // Original DrawFade will disable344 after rendering the transparent endpoint.
    }
    void GuestIsolation::Retire() noexcept
    {
        if(!state_ || state_->retired)return;
        state_->Trace();ZeroMemory(state_->motion,sizeof(state_->motion));state_->record=0;
        state_->bound=state_->sceneReady=false;state_->retired=true;
        state_->flow.Retire();
        OutputDebugStringA("WinxCoop guest isolation scene retired: motion receivers cleared; save/effect guards retained until exit");
    }
    void GuestIsolation::CloseBeforeHooks() noexcept
    {
        if(!state_ || state_->published)return;
        if(state_->code)VirtualFree(state_->code,0,MEM_RELEASE);
        state_->~State();HeapFree(GetProcessHeap(),0,state_);state_=nullptr;
    }
}
