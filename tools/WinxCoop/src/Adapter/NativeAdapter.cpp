// Own pinned-image x86 adapter. Observer is the default. Optional passive
// commands execute on the original main thread; no player/controller is made.
#include "RuntimeContract.h"
#include "PinnedImage.h"
#include "PassiveActor.h"
#include <bcrypt.h>

using namespace winxcoop::adapter;
namespace
{
    constexpr std::uintptr_t WorldReturn = 0x45A802;
    RuntimeShared* shared{};
    HANDLE mapping{};
    PVOID handler{};
    DWORD mainThread{};
    RuntimeSnapshot snapshot{};
    ULONGLONG lastPublished{};
    PinnedImage pinnedImage;
    PassiveActor actor;
    RuntimeStart configuration{};
    RuntimeCommand lastCommand{};
    RuntimeAck lastAck{};
    HANDLE owner{}, assets[5]{};
    std::uint32_t actorProfile{}, actorAnimationManager{};
    std::uint32_t observationReason{}, lastPausedReason{};
    enum class SafePhase { None, World, PreUnload };
    SafePhase safePhase{};
    struct UnloadGuard final { std::uint32_t address; BYTE instruction; BYTE signature[16]; bool installed; };
    UnloadGuard unloadGuards[] = {
        {0x5F86C0,0x53,{},false}, {0x4E8AA0,0x53,{},false}, {0x4E8C80,0x56,{},false},
        {0x59AE80,0x51,{},false}, {0x59B190,0x51,{},false}};
    volatile LONG inHandler{}, terminalError{}, firstWorldLogged{};
    void TraceWorld(const CONTEXT& context, const std::uint32_t caller) noexcept
    {
        char line[]="WinxCoop first-world eip=00000000 esp=00000000 caller=00000000 tid=00000000 expectedTid=00000000";
        const char* hex="0123456789ABCDEF";
        const std::uint32_t values[]={context.Eip,context.Esp,caller,GetCurrentThreadId(),mainThread};
        const unsigned offsets[]={25,38,54,67,88};
        for(unsigned word=0;word<5;++word)for(unsigned digit=0;digit<8;++digit)
            line[offsets[word]+digit]=hex[(values[word]>>(28-digit*4))&15];
        OutputDebugStringA(line);
        char fp[]="WinxCoop first-world-fp flags=00000000 cw=00000000 sw=00000000 tag=00000000 fxcw=00000000 fxtag=00000000 mxcsr=00000000 xmm0=00000000 xmm7=00000000";
        std::uint32_t fxcw{},mxcsr{},xmm0{},xmm7{};
        CopyMemory(&fxcw,context.ExtendedRegisters,2);
        CopyMemory(&mxcsr,context.ExtendedRegisters+24,4);
        CopyMemory(&xmm0,context.ExtendedRegisters+160,4);
        CopyMemory(&xmm7,context.ExtendedRegisters+272,4);
        const std::uint32_t fpValues[]={context.ContextFlags,context.FloatSave.ControlWord,context.FloatSave.StatusWord,
            context.FloatSave.TagWord,fxcw,context.ExtendedRegisters[4],mxcsr,xmm0,xmm7};
        const unsigned fpOffsets[]={30,42,54,67,81,96,111,125,139};
        for(unsigned word=0;word<9;++word)for(unsigned digit=0;digit<8;++digit)
            fp[fpOffsets[word]+digit]=hex[(fpValues[word]>>(28-digit*4))&15];
        OutputDebugStringA(fp);
    }

    bool Read(const std::uintptr_t address, void* output, const SIZE_T bytes) noexcept
    {
        SIZE_T got{};
        return address >= 0x10000 && ReadProcessMemory(GetCurrentProcess(), reinterpret_cast<const void*>(address), output, bytes, &got) && got == bytes;
    }
    std::uint32_t Word(const std::uintptr_t address) noexcept
    {
        std::uint32_t result{};
        Read(address, &result, sizeof(result));
        return result;
    }
    bool Finite(const float value) noexcept
    {
        std::uint32_t bits{}; CopyMemory(&bits, &value, sizeof(bits));
        return (bits & 0x7f800000) != 0x7f800000;
    }
    bool Same(const void* a, const void* b, const SIZE_T bytes) noexcept
    {
        const auto* left=static_cast<const BYTE*>(a);const auto* right=static_cast<const BYTE*>(b);
        for(SIZE_T i=0;i<bytes;++i)if(left[i]!=right[i])return false;
        return true;
    }
    bool VerifyNativeSource(const std::uint32_t at, const std::size_t bytes) noexcept { return pinnedImage.Verify(at,bytes); }
    bool ActorPhase() noexcept
    { return safePhase!=SafePhase::None && GetCurrentThreadId()==mainThread && InterlockedCompareExchange(&inHandler,0,0)==1; }
    bool PassiveMode() noexcept { return configuration.mode==RuntimeMode::PassiveActor; }
    void Statistics() noexcept
    {
        const auto stats=actor.Statistics();snapshot.remoteCreated=stats.created;snapshot.remoteRemoved=stats.destroyed;
        snapshot.nativeCalls=stats.nativeCalls;snapshot.actorError=static_cast<std::uint32_t>(actor.Error());
    }
    void FailClosed(const RuntimeError error) noexcept
    {
        const LONG previous=InterlockedCompareExchange(&terminalError,static_cast<LONG>(error),0);
        if(GetCurrentThreadId()==mainThread)
        {
            snapshot.phase=RuntimePhase::Failed;snapshot.error=previous?static_cast<RuntimeError>(previous):error;
            snapshot.monotonicMilliseconds=GetTickCount64();Statistics();
            if(shared)Publish(shared->snapshot,snapshot);
        }
        OutputDebugStringA("WinxCoop terminal runtime gate failed; terminating owned game");
        // Never unwind a partially qualified native transaction into the game.
        TerminateProcess(GetCurrentProcess(),126);
    }
    bool MediaText(const char* text, std::uint32_t& length) noexcept
    {
        length=0;
        while(length<MAX_PATH && text[length])
        {const auto c=static_cast<unsigned char>(text[length]);if(c<32 || c>126 || c=='"')return false;++length;}
        if(length<4 || length>=MAX_PATH || text[1]!=':' || text[2]!='\\' ||
            !((text[0]>='A' && text[0]<='Z') || (text[0]>='a' && text[0]<='z')))return false;
        while(length>3 && (text[length-1]=='\\' || text[length-1]=='/'))--length;
        return true;
    }
    bool MediaMatchesNative() noexcept
    {
        const auto manager=Word(0x755280);if(Word(manager)!=0x7027E0)return false;
        const auto text=Word(manager+0x14);char actual[MAX_PATH]{};
        if(text<0x10000)return false;
        for(std::uint32_t i=0;i<MAX_PATH;++i)
        {if(!Read(text+i,actual+i,1))return false;if(!actual[i])break;}
        std::uint32_t a{},b{};
        if(!MediaText(actual,a) || !MediaText(configuration.mediaDirectory,b) || a!=b)return false;
        for(std::uint32_t i=0;i<a;++i)
        {
            char x=actual[i],y=configuration.mediaDirectory[i];
            if(x>='A' && x<='Z')x+=32;if(y>='A' && y<='Z')y+=32;
            if(x!=y)return false;
        }
        return true;
    }
    void CloseResourcesBeforeHooks() noexcept
    {
        for(auto& file:assets)if(file){CloseHandle(file);file=nullptr;}
        if(owner){CloseHandle(owner);owner=nullptr;}
        if(shared){UnmapViewOfFile(shared);shared=nullptr;}
        if(mapping){CloseHandle(mapping);mapping=nullptr;}
        pinnedImage.Close();
    }
    bool PinAssets() noexcept
    {
        static const char* names[]={"bloom_jeans.smo","bloom_hair.smo","blidsh.san","blwa.san","bljusm.san"};
        static const char* hashes[]={
            "17184190AAB5C45CFE7EB594B6B166A343CD688D99C236FEA3508CA5809B28CA",
            "AF68FF609294648CB817497868BB6D0F222863F76D62AC19CA9E39D480702654",
            "04BBAFAF20DB422AC8A110D6CDACF44B44897571E64ED21E8424C2E4524DB725",
            "E4A3DF69FBA8353CC2DD5928AFEC4490512F10002C9FE843AE555F0FCAFAC2B0",
            "C130E5403DA2D0F7F5D385CB82FD1DB02368B6E4CB18DC37A03FFFC0041E96DF"};
        std::uint32_t length{};if(!MediaText(configuration.mediaDirectory,length))return false;
        BCRYPT_ALG_HANDLE algorithm{};
        if(BCryptOpenAlgorithmProvider(&algorithm,BCRYPT_SHA256_ALGORITHM,nullptr,0)<0)return false;
        bool result=true;
        for(unsigned i=0;i<5 && result;++i)
        {
            char path[MAX_PATH]{};CopyMemory(path,configuration.mediaDirectory,length);
            const char suffix[]="\\Characters\\Bloom\\";unsigned n=length;
            for(const char c:suffix){if(!c)break;if(n+1>=MAX_PATH){result=false;break;}path[n++]=c;}
            for(unsigned j=0;result && names[i][j];++j){if(n+1>=MAX_PATH){result=false;break;}path[n++]=names[i][j];}
            if(!result)break;
            HANDLE file=CreateFileA(path,GENERIC_READ,FILE_SHARE_READ,nullptr,OPEN_EXISTING,FILE_ATTRIBUTE_NORMAL,nullptr);
            if(file==INVALID_HANDLE_VALUE){result=false;break;}assets[i]=file;
            LARGE_INTEGER size{};BCRYPT_HASH_HANDLE hash{};BYTE digest[32]{},buffer[16384]{};
            result=GetFileSizeEx(file,&size) && size.QuadPart>0 && size.QuadPart<=64*1024*1024 &&
                BCryptCreateHash(algorithm,&hash,nullptr,0,nullptr,0,0)>=0;
            LONGLONG consumed{};
            while(result && consumed<size.QuadPart)
            {
                const LONGLONG remaining=size.QuadPart-consumed;const DWORD want=remaining>16384?16384:static_cast<DWORD>(remaining);
                DWORD got{};result=ReadFile(file,buffer,want,&got,nullptr) && got==want && BCryptHashData(hash,buffer,got,0)>=0;consumed+=got;
            }
            result=result && BCryptFinishHash(hash,digest,sizeof(digest),0)>=0;
            if(hash)BCryptDestroyHash(hash);
            constexpr char hex[]="0123456789ABCDEF";
            for(unsigned j=0;result && j<32;++j)result=hashes[i][j*2]==hex[digest[j]>>4] && hashes[i][j*2+1]==hex[digest[j]&15];
        }
        BCryptCloseAlgorithmProvider(algorithm,0);return result;
    }
    bool GuardSignature(const UnloadGuard& guard) noexcept
    {
        BYTE live[16]{};if(!Read(guard.address,live,sizeof(live)))return false;
        if(live[0]!=(guard.installed?0xCC:guard.instruction))return false;
        return Same(live+1,guard.signature+1,sizeof(live)-1);
    }
    bool InstallByte(const std::uint32_t address) noexcept
    {
        DWORD protection{},ignored{};
        if(!VirtualProtect(reinterpret_cast<void*>(address),1,PAGE_EXECUTE_READWRITE,&protection))return false;
        *reinterpret_cast<volatile BYTE*>(address)=0xCC;
        const bool flushed=FlushInstructionCache(GetCurrentProcess(),reinterpret_cast<void*>(address),1)!=FALSE;
        const bool restored=VirtualProtect(reinterpret_cast<void*>(address),1,protection,&ignored)!=FALSE;
        return flushed && restored;
    }
    bool Observe() noexcept
    {
        observationReason=0;
        const auto profile = Word(0x765AD4), record = Word(profile + 0x2B4), root = Word(record + 0x24);
        const auto flow = Word(0x755294), sceneManager = Word(0x75DB90);
        std::uint32_t level{}, pending{}, stack{}, currentScene{};
        if (flow < 0x10000 || sceneManager < 0x10000 ||
            !Read(flow + 0x1B0,&level,4) || !Read(flow + 0x1B8,&pending,4) ||
            !Read(flow + 0x1AC,&stack,4) || !Read(sceneManager + 0x20,&currentScene,4)) {observationReason=1;return false;}
        const auto active = stack <= 20 ? Word(flow + 0x15C + stack * 4) : 0;
        if (Word(profile) != 0x6F5A44 || Word(record) != 0x710F80 || Word(root) != 0x6DC4F4 ||
            level != 28 || pending != 0 || Word(active + 0x10) != level || currentScene != 0 ||
            Word(root + 0x3C) == 0 || Word(Word(root + 0x3C)) != 0x6E7358 || Word(record + 0x20) != 11)
        {
            observationReason=2;
            if(pending)observationReason|=4;
            if(Word(active+0x10)!=level)observationReason|=8;
            return false;
        }
        Transform transform{};
        if (!Read(root + 0x20, transform.position, 12) || !Read(root + 0x30, transform.scale, 12) ||
            !Read(root + 0x40, transform.orientation, 36)) return false;
        for (float value : transform.position) if (!Finite(value)) return false;
        for (float value : transform.scale) if (!Finite(value)) return false;
        for (float value : transform.orientation) if (!Finite(value)) return false;
        snapshot.local = transform;
        snapshot.levelId = level;
        snapshot.localComponentCount = 11;
        return true;
    }
    bool SameCommand(const RuntimeCommand& a, const RuntimeCommand& b) noexcept
    {
        return a.bytes==b.bytes && a.version==b.version && a.session==b.session && a.levelEpoch==b.levelEpoch &&
            a.sequence==b.sequence && a.entity==b.entity && a.operation==b.operation && a.animation==b.animation &&
            a.reserved==b.reserved && Same(&a.remote,&b.remote,sizeof(a.remote));
    }
    bool CommandTransform(const Transform& transform) noexcept
    {
        for(float x:transform.position)if(!Finite(x))return false;
        for(float x:transform.scale)if(!Finite(x) || x<=0)return false;
        for(float x:transform.orientation)if(!Finite(x))return false;
        return true; // PassiveActor independently qualifies matrix and stock scale.
    }
    bool Commands(const bool ready) noexcept
    {
        RuntimeCommand command{};
        if(!TryRead(shared->command,command))return true;
        if(!command.sequence)
        {
            RuntimeCommand empty{};
            return SameCommand(command,empty) && !lastCommand.sequence;
        }
        if(command.bytes!=sizeof(command) || command.version!=IpcVersion || command.session!=configuration.session ||
            command.levelEpoch!=configuration.levelEpoch || !command.entity || command.entity>2 || command.reserved ||
            command.operation<RuntimeOperation::Create || command.operation>RuntimeOperation::Remove ||
            command.animation>RuntimeAnimation::Jump || !PassiveMode())return false;
        if(command.sequence==lastCommand.sequence)
        {
            if(!SameCommand(command,lastCommand))return false;
            return true; // Already acknowledged, never execute twice.
        }
        if(lastCommand.sequence==0xFFFFFFFF || command.sequence!=lastCommand.sequence+1 || snapshot.epochRetired)return false;
        RuntimeAck previous{};
        if(!TryRead(shared->acknowledgement,previous) || !Same(&previous,&lastAck,sizeof(lastAck)))return false;
        if(!ready)return true; // Hold the same next command; no native call or ACK during pause.
        bool applied=false;
        if(command.operation==RuntimeOperation::Create)
        {
            if(actor.Created() || snapshot.remoteEntity || !CommandTransform(command.remote) || !MediaMatchesNative())return false;
            actorProfile=Word(0x765AD4);actorAnimationManager=Word(0x765ADC);
            if(actorProfile<0x10000 || Word(actorAnimationManager)!=0x703470)return false;
            applied=actor.Create(configuration.mediaDirectory) && actor.ApplyRemote(command.remote,static_cast<PassiveAnimation>(command.animation));
            if(applied)snapshot.remoteEntity=command.entity;
        }
        else
        {
            if(!actor.Created() || snapshot.remoteEntity!=command.entity || Word(0x765AD4)!=actorProfile ||
                Word(0x765ADC)!=actorAnimationManager || !MediaMatchesNative())return false;
            if(command.operation==RuntimeOperation::Apply)
                applied=CommandTransform(command.remote) && actor.ApplyRemote(command.remote,static_cast<PassiveAnimation>(command.animation));
            else
            {
                applied=actor.DestroyBeforeUnload() && !actor.Created();
                if(applied){snapshot.remoteEntity=0;actorProfile=actorAnimationManager=0;}
            }
        }
        Statistics();
        lastCommand=command;snapshot.lastCommandSequence=command.sequence;
        lastAck={};lastAck.bytes=sizeof(lastAck);lastAck.version=IpcVersion;lastAck.session=command.session;
        lastAck.levelEpoch=command.levelEpoch;lastAck.sequence=command.sequence;lastAck.entity=command.entity;
        lastAck.operation=command.operation;lastAck.status=applied?RuntimeAckStatus::Applied:RuntimeAckStatus::Failed;
        lastAck.error=applied?RuntimeError::None:RuntimeError::Actor;lastAck.worldUpdates=snapshot.worldUpdates;lastAck.actorError=snapshot.actorError;
        Publish(shared->acknowledgement,lastAck);
        if(!applied)FailClosed(RuntimeError::Actor);
        return applied;
    }
    void Tick() noexcept
    {
        ++snapshot.worldUpdates;
        const auto now = GetTickCount64();
        if(!owner || WaitForSingleObject(owner,0)!=WAIT_TIMEOUT || shared->magic!=IpcMagic || shared->version!=IpcVersion ||
            shared->bytes!=sizeof(*shared) || shared->nonce!=configuration.nonce || shared->launcherPid!=GetProcessId(owner))
            {FailClosed(RuntimeError::Owner);return;}
        const bool ready=Observe();
        if(!snapshot.epochRetired)snapshot.phase=ready?RuntimePhase::Ready:RuntimePhase::WaitingForLevel;
        if(PassiveMode())
        {
            for(const auto& guard:unloadGuards)if(!guard.installed || !GuardSignature(guard)){FailClosed(RuntimeError::Signature);return;}
            const bool mediaMatches=MediaMatchesNative();
            snapshot.mediaQualified=mediaMatches?1:0;
            if(actor.Created())
            {
                // Command readiness is narrower than lifetime. A flow overlay
                // may pause an intact scene before any native unload entry.
                const bool owned=actor.ValidateLiveOwnership();
                if(!owned || snapshot.epochRetired || !mediaMatches || Word(0x765AD4)!=actorProfile || Word(0x765ADC)!=actorAnimationManager)
                    {FailClosed(RuntimeError::Unload);return;}
                if(!ready && lastPausedReason!=observationReason)
                {
                    char text[384]{};char* cursor=text;
                    const auto append=[&cursor](const char* value) noexcept {while(*value)*cursor++=*value++;};
                    const auto word=[&cursor,&append](const char* label,std::uint32_t value) noexcept
                    {append(label);for(unsigned i=0;i<8;++i)*cursor++="0123456789ABCDEF"[(value>>(28-i*4))&15];};
                    const auto flow=Word(0x755294),stack=Word(flow+0x1AC),active=stack<=20?Word(flow+0x15C+stack*4):0;
                    append("WinxCoop live-scene held while not ready");word(" reason=",observationReason);word(" flow=",flow);
                    word(" stack=",stack);word(" pending=",Word(flow+0x1B8));word(" active=",active);
                    word(" activeType=",Word(active));word(" activeLevel=",Word(active+0x10));
                    word(" level=",Word(flow+0x1B0));word(" media=",mediaMatches?1:0);
                    OutputDebugStringA(text);lastPausedReason=observationReason;
                }
                if(ready)lastPausedReason=0;
            }
            if(!Commands(ready && !snapshot.epochRetired)){FailClosed(RuntimeError::Command);return;}
        }
        else
        {
            RuntimeCommand empty{},command{};
            if(TryRead(shared->command,command) && !SameCommand(command,empty)){FailClosed(RuntimeError::Command);return;}
        }
        if(InterlockedCompareExchange(&terminalError,0,0))return;
        if (now - lastPublished < 50) return;lastPublished = now;
        snapshot.monotonicMilliseconds = now;
        snapshot.phase = snapshot.epochRetired?RuntimePhase::Stopped:ready?RuntimePhase::Ready:RuntimePhase::WaitingForLevel;
        Statistics();
        Publish(shared->snapshot, snapshot);
    }
    bool PreUnload(UnloadGuard& guard, CONTEXT& context) noexcept
    {
        if(snapshot.unloadGuardHits<128 || actor.Created())
        {
            char text[]="WinxCoop pre-unload entry=00000000 ecx=00000000 esp=00000000 live=00000000";
            const char* hex="0123456789ABCDEF";
            const std::uint32_t words[]={guard.address,context.Ecx,context.Esp,actor.Created()?1u:0u};
            const unsigned offsets[]={26,39,52,66};
            for(unsigned i=0;i<4;++i)for(unsigned j=0;j<8;++j)text[offsets[i]+j]=hex[(words[i]>>(28-j*4))&15];
            OutputDebugStringA(text);
        }
        if(!GuardSignature(guard) || context.Eip!=guard.address || context.Esp<0x10004 || (context.Esp&3) || (context.EFlags&0x100))return false;
        ++snapshot.unloadGuardHits;
        if(actor.Created())
        {
            if(Word(0x765AD4)!=actorProfile || Word(0x765ADC)!=actorAnimationManager || !MediaMatchesNative())return false;
            if((guard.address==0x4E8AA0 || guard.address==0x4E8C80) && context.Ecx!=actorProfile)return false;
            if((guard.address==0x59AE80 || guard.address==0x59B190) && context.Ecx!=actorAnimationManager)return false;
            if(guard.address==0x5F86C0)
            {
                const auto flow=Word(0x755294),stack=Word(flow+0x1AC);
                if(stack>20 || Word(context.Ecx)!=0x710874 || Word(flow+0x15C+stack*4)!=context.Ecx)return false;
            }
            safePhase=SafePhase::PreUnload;
            const bool removed=actor.DestroyBeforeUnload() && !actor.Created() && !actor.Poisoned();
            safePhase=SafePhase::None;Statistics();
            if(!removed)return false;
            ++snapshot.unloadCleanups;snapshot.remoteEntity=0;actorProfile=actorAnimationManager=0;
            OutputDebugStringA("WinxCoop passive actor removed before native unload entry");
        }
        // Any destructive boundary after actor use retires this cohort. A new
        // level/session needs a fresh owned runtime, never stale borrowed clips.
        if(snapshot.remoteCreated)
        {
            snapshot.epochRetired=1;snapshot.phase=RuntimePhase::Stopped;snapshot.monotonicMilliseconds=GetTickCount64();
            Publish(shared->snapshot,snapshot);
        }
        // Permanent hook, exact original PUSH semantics. No single-step flag,
        // no temporary restoration/rearm interval where another unload can pass.
        const std::uint32_t pushed=guard.instruction==0x53?context.Ebx:guard.instruction==0x56?context.Esi:context.Ecx;
        MEMORY_BASIC_INFORMATION region{};
        if(!VirtualQuery(reinterpret_cast<const void*>(context.Esp-4),&region,sizeof(region)) || region.State!=MEM_COMMIT ||
            region.Type!=MEM_PRIVATE || (region.Protect&PAGE_GUARD) ||
            (region.Protect&0xFF)!=PAGE_READWRITE || context.Esp-4<reinterpret_cast<std::uintptr_t>(region.BaseAddress) ||
            static_cast<std::uint64_t>(context.Esp)>reinterpret_cast<std::uintptr_t>(region.BaseAddress)+region.RegionSize)return false;
        SIZE_T wrote{};std::uint32_t readback{};
        if(!WriteProcessMemory(GetCurrentProcess(),reinterpret_cast<void*>(context.Esp-4),&pushed,4,&wrote) || wrote!=4 ||
            !Read(context.Esp-4,&readback,4) || readback!=pushed)return false;
        context.Esp-=4;context.Eip=guard.address+1;return true;
    }
    LONG CALLBACK ExceptionHandler(EXCEPTION_POINTERS* exception)
    {
        if (!exception || !exception->ExceptionRecord || !exception->ContextRecord ||
            (exception->ExceptionRecord->ExceptionCode != EXCEPTION_BREAKPOINT && exception->ExceptionRecord->ExceptionCode != 0x4000001F) || !shared)
            return EXCEPTION_CONTINUE_SEARCH;
        const auto at=reinterpret_cast<std::uintptr_t>(exception->ExceptionRecord->ExceptionAddress);
        UnloadGuard* guard{};
        if(PassiveMode())for(auto& candidate:unloadGuards)if(candidate.installed && candidate.address==at)guard=&candidate;
        if(at!=WorldReturn && !guard)return EXCEPTION_CONTINUE_SEARCH;
        if (at==WorldReturn && InterlockedCompareExchange(&firstWorldLogged,1,0)==0)
        {
            std::uint32_t observedCaller{};
            Read(exception->ContextRecord->Esp,&observedCaller,4);
            TraceWorld(*exception->ContextRecord,observedCaller);
        }
        if (GetCurrentThreadId() != mainThread || InterlockedCompareExchange(&inHandler,1,0) != 0 ||
            InterlockedCompareExchange(&terminalError,0,0) != 0)
        {
            // A foreign thread must not become a second mailbox writer. End
            // this owned diagnostic before another game instruction can run.
            FailClosed(RuntimeError::Thread);
            return EXCEPTION_CONTINUE_SEARCH;
        }
        if(guard)
        {
            bool completed=false;
            __try { completed=PreUnload(*guard,*exception->ContextRecord); }
            __except(EXCEPTION_EXECUTE_HANDLER) { FailClosed(RuntimeError::NativeException); }
            if(!completed || InterlockedCompareExchange(&terminalError,0,0))
            {FailClosed(RuntimeError::Unload);return EXCEPTION_CONTINUE_SEARCH;}
            InterlockedExchange(&inHandler,0);return EXCEPTION_CONTINUE_EXECUTION;
        }
        std::uint32_t caller{};
        // The x86 debugger sees EIP after INT3. The application's dispatched
        // VEH context has EIP rewound to the faulting instruction (measured
        // independently in adapter-observe-v4). Do not mix those two APIs.
        BYTE worldInstruction{};
        if (exception->ContextRecord->Eip != WorldReturn || !Read(WorldReturn,&worldInstruction,1) || worldInstruction!=0xCC ||
            !Read(exception->ContextRecord->Esp, &caller, sizeof(caller)) || caller != 0x41CDF8)
        {
            FailClosed(RuntimeError::Memory);
            return EXCEPTION_CONTINUE_SEARCH;
        }
        safePhase=SafePhase::World;
        __try { Tick(); }
        __except(EXCEPTION_EXECUTE_HANDLER) { FailClosed(RuntimeError::NativeException); }
        safePhase=SafePhase::None;
        if(InterlockedCompareExchange(&terminalError,0,0))return EXCEPTION_CONTINUE_SEARCH;
        exception->ContextRecord->Eip = caller;
        exception->ContextRecord->Esp += 4; // Exact semantics of the verified C3.
        InterlockedExchange(&inHandler,0);
        return EXCEPTION_CONTINUE_EXECUTION;
    }
}
extern "C" __declspec(dllexport) DWORD WINAPI WinxCoopStartAdapter(void* argument)
{
    if (shared || !argument) return static_cast<DWORD>(RuntimeError::Identity);
    RuntimeStart start{};
    if (!Read(reinterpret_cast<std::uintptr_t>(argument), &start, sizeof(start)) || start.bytes != sizeof(start) ||
        start.version != IpcVersion || start.gamePid != GetCurrentProcessId() || !start.mainThreadId || !start.nonce ||
        start.mappingName[127] != 0 || !start.mappingName[0] || GetCurrentThreadId()!=start.mainThreadId ||
        start.mode>RuntimeMode::PassiveActor || (start.mode==RuntimeMode::PassiveActor && (!start.session || !start.levelEpoch)) ||
        (start.mode==RuntimeMode::Observe && (start.session || start.levelEpoch || start.mediaDirectory[0])))
        return static_cast<DWORD>(RuntimeError::Identity);
    configuration=start;
    if (!pinnedImage.Open()) return static_cast<DWORD>(RuntimeError::Image);
    const auto* image = reinterpret_cast<const IMAGE_DOS_HEADER*>(GetModuleHandleW(nullptr));
    if (reinterpret_cast<std::uintptr_t>(image) != 0x400000 || image->e_magic != IMAGE_DOS_SIGNATURE)
        {CloseResourcesBeforeHooks();return static_cast<DWORD>(RuntimeError::Image);}
    const auto* pe = reinterpret_cast<const IMAGE_NT_HEADERS*>(reinterpret_cast<const BYTE*>(image) + image->e_lfanew);
    if (pe->Signature != IMAGE_NT_SIGNATURE || pe->FileHeader.Machine != IMAGE_FILE_MACHINE_I386 ||
        pe->OptionalHeader.ImageBase != 0x400000 || !pinnedImage.Verify(WorldReturn,1) || *reinterpret_cast<const BYTE*>(WorldReturn) != 0xC3)
        {CloseResourcesBeforeHooks();return static_cast<DWORD>(RuntimeError::Signature);}
    mapping = OpenFileMappingW(FILE_MAP_READ | FILE_MAP_WRITE, FALSE, start.mappingName);
    if (!mapping) {CloseResourcesBeforeHooks();return static_cast<DWORD>(RuntimeError::Ipc);}
    shared = static_cast<RuntimeShared*>(MapViewOfFile(mapping, FILE_MAP_READ | FILE_MAP_WRITE, 0, 0, sizeof(RuntimeShared)));
    if (!shared || shared->magic != IpcMagic || shared->version != IpcVersion || shared->bytes != sizeof(RuntimeShared) || shared->nonce != start.nonce)
    {
        CloseResourcesBeforeHooks();
        return static_cast<DWORD>(RuntimeError::Ipc);
    }
    owner=OpenProcess(SYNCHRONIZE|PROCESS_QUERY_LIMITED_INFORMATION,FALSE,shared->launcherPid);
    if(!owner || shared->launcherPid==GetCurrentProcessId() || GetProcessId(owner)!=shared->launcherPid || WaitForSingleObject(owner,0)!=WAIT_TIMEOUT)
    {CloseResourcesBeforeHooks();return static_cast<DWORD>(RuntimeError::Owner);}
    mainThread = start.mainThreadId;
    snapshot.gamePid = start.gamePid; snapshot.mainThreadId = mainThread; snapshot.phase = RuntimePhase::Attached;
    snapshot.mode=start.mode;snapshot.session=start.session;snapshot.levelEpoch=start.levelEpoch;
    if(PassiveMode())
    {
        if(!PinAssets()){CloseResourcesBeforeHooks();return static_cast<DWORD>(RuntimeError::Media);}
        for(auto& guard:unloadGuards)
            if(!pinnedImage.Verify(guard.address,sizeof(guard.signature)) || !Read(guard.address,guard.signature,sizeof(guard.signature)) || guard.signature[0]!=guard.instruction)
            {CloseResourcesBeforeHooks();return static_cast<DWORD>(RuntimeError::Signature);}
        const PassiveActorContext context{mainThread,VerifyNativeSource,ActorPhase};
        if(!actor.Configure(context)){CloseResourcesBeforeHooks();return static_cast<DWORD>(RuntimeError::Actor);}
    }
    handler = AddVectoredExceptionHandler(1, ExceptionHandler);
    if (!handler)
    {
        CloseResourcesBeforeHooks();
        return static_cast<DWORD>(RuntimeError::Signature);
    }
    // After the first installed hook any failure is terminal. Do not remove the
    // VEH, unload the DLL, or release borrowed resources from loader-lock code.
    if(!InstallByte(WorldReturn)){FailClosed(RuntimeError::Signature);return static_cast<DWORD>(RuntimeError::Signature);}
    if(PassiveMode())for(auto& guard:unloadGuards)
    {
        if(!InstallByte(guard.address)){FailClosed(RuntimeError::Signature);return static_cast<DWORD>(RuntimeError::Signature);}
        guard.installed=true;
    }
    Publish(shared->snapshot, snapshot);
    return 0;
}
BOOL WINAPI DllMain(HINSTANCE, DWORD, LPVOID)
{
    // /MT requires CRT thread notifications. Do not disable them here.
    // The owner keeps the module loaded until its own game exits. Loader-lock
    // detach performs no game calls, waits, IPC or destruction of native owners.
    return TRUE;
}
