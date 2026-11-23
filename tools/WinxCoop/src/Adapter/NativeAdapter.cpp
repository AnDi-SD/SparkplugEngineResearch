// Own pinned-image x86 adapter. Observer is the default. Optional passive
// commands execute on the original main thread; no player/controller is made.
#include "RuntimeContract.h"
#include "PinnedImage.h"
#include "PassiveActor.h"
#include "PassivePoseSmoother.h"
#include "GuestIsolation.h"
#include "GuestSceneTransition.h"
#include "PassiveImageCompatibility.h"
#include <cmath>

using namespace winxcoop::adapter;
namespace
{
    constexpr std::uintptr_t WorldReturn = 0x45A802;
    constexpr std::uint32_t GuestSettingsReturn = 0x40E373;
    RuntimeShared* shared{};
    HANDLE mapping{};
    PVOID handler{};
    DWORD mainThread{};
    RuntimeSnapshot snapshot{};
    ULONGLONG lastPublished{};
    PinnedImage pinnedImage;
    PassiveActor actor;
    GuestIsolation guestIsolation;
    GuestSceneTransition guestTransition;
    PassivePoseSmoother remoteInterpolation;
    RuntimeAnimation remoteAnimation{};
    std::uint64_t lastDisplayTime{};
    RuntimeStart configuration{};
    RuntimeCommand lastCommand{};
    RuntimeAck lastAck{};
    RuntimeCommand pendingFollow{};
    bool followPending{};
    HANDLE owner{}, assets[5]{};
    std::uint32_t actorProfile{}, actorAnimationManager{};
    std::uint32_t sceneProfile{}, sceneAnimationManager{}, sceneRecord{}, sceneRoot{}, scenePlayingState{};
    std::uint32_t sceneFlow{}, sceneSceneManager{}, sceneLevelId{};
    bool qualifiedSceneSeen{};
    bool sceneRearmEligible{}, retiredPlayingExitSeen{};
    std::uint32_t retiredPlayingState{}, retiredFlow{}, retiredAtWorld{};
    bool guestStartupInstalled{}, guestStartupApplied{};
    BYTE guestStartupSignature[16]{};
    const char* guestStartupFailure{"not applied"};
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
    bool GuestMode() noexcept { return PassiveMode() && configuration.startupLevel!=0; }
    bool SceneFollowing() noexcept { return PassiveMode() && configuration.sceneFollow==1; }
    void TraceLifetimeFailure() noexcept
    {
        char text[384]{};char* cursor=text;char* const end=text+sizeof(text)-1;
        const auto append=[&cursor,end](const char* value) noexcept {while(*value && cursor<end)*cursor++=*value++;};
        const auto word=[&cursor,&append,end](const char* label,std::uint32_t value) noexcept
        {append(label);for(unsigned i=0;i<8 && cursor<end;++i)*cursor++="0123456789ABCDEF"[(value>>(28-i*4))&15];};
        const auto flow=Word(0x755294),stack=Word(flow+0x1AC),active=stack<=20?Word(flow+0x15C+stack*4):0;
        const auto profile=Word(0x765AD4),record=Word(profile+0x2B4),root=Word(record+0x24);
        append("WinxCoop lifetime-failure");word(" reason=",observationReason);word(" flow=",flow);word(" stack=",stack);
        word(" pending=",Word(flow+0x1B8));word(" top=",active);word(" topType=",Word(active));
        word(" topId=",Word(active+0x10));word(" current=",Word(flow+0x1B0));word(" root=",root);
        word(" scene=",Word(root+0x3C));word(" actorError=",static_cast<std::uint32_t>(actor.Error()));
        append(" detail=");append(actor.LastError());OutputDebugStringA(text);
    }
    void Statistics() noexcept
    {
        const auto stats=actor.Statistics();snapshot.remoteCreated=stats.created;snapshot.remoteRemoved=stats.destroyed;
        snapshot.nativeCalls=stats.nativeCalls;snapshot.actorError=static_cast<std::uint32_t>(actor.Error());
    }
    void TraceDetail(const char* prefix,const char* detail) noexcept
    {
        // Record the precise bounded actor gate, rather than only its numeric
        // category. This reads our own retained error text, not the game graph.
        char text[384]{};SIZE_T count=0;
        const auto append=[&](const char* value) noexcept
        {if(value)while(*value && count<sizeof(text)-1)text[count++]=*value++;};
        append(prefix);append(detail);
        OutputDebugStringA(text);
    }
    void FailClosed(const RuntimeError error) noexcept
    {
        const LONG previous=InterlockedCompareExchange(&terminalError,static_cast<LONG>(error),0);
        if(GetCurrentThreadId()==mainThread)
        {
            if(PassiveMode() && actor.Error()!=PassiveActorError::None)
                TraceDetail("WinxCoop passive-actor failure: ",actor.LastError());
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
    void CloseAssetPins() noexcept
    {
        for(auto& file:assets)if(file){CloseHandle(file);file=nullptr;}
    }
    void CloseResourcesBeforeHooks() noexcept
    {
        guestIsolation.CloseBeforeHooks();
        CloseAssetPins();
        if(owner){CloseHandle(owner);owner=nullptr;}
        if(shared){UnmapViewOfFile(shared);shared=nullptr;}
        if(mapping){CloseHandle(mapping);mapping=nullptr;}
        pinnedImage.Close();
    }
    bool PinAssets() noexcept
    {
        static const char* names[]={"bloom_jeans.smo","bloom_hair.smo","blidsh.san","blwa.san","bljusm.san"};
        // Called only by a future explicit visual Create, never by ordinary
        // game startup. Contents may be modded; native graph/clip/type and
        // ownership checks remain the actual functional admission boundary.
        if(assets[0]){for(const auto file:assets)if(!file)return false;return true;}
        std::uint32_t length{};if(!MediaText(configuration.mediaDirectory,length))return false;
        bool result=true;
        for(unsigned i=0;i<5 && result;++i)
        {
            char path[MAX_PATH]{};CopyMemory(path,configuration.mediaDirectory,length);
            const char suffix[]="\\Characters\\Bloom\\";unsigned n=length;
            for(const char c:suffix){if(!c)break;if(n+1>=MAX_PATH){result=false;break;}path[n++]=c;}
            for(unsigned j=0;result && names[i][j];++j){if(n+1>=MAX_PATH){result=false;break;}path[n++]=names[i][j];}
            if(!result)break;
            HANDLE file=CreateFileA(path,GENERIC_READ,FILE_SHARE_READ,nullptr,OPEN_EXISTING,FILE_ATTRIBUTE_NORMAL|FILE_FLAG_OPEN_REPARSE_POINT,nullptr);
            if(file==INVALID_HANDLE_VALUE){result=false;break;}assets[i]=file;
            LARGE_INTEGER size{};BY_HANDLE_FILE_INFORMATION info{};
            result=GetFileType(file)==FILE_TYPE_DISK && GetFileInformationByHandle(file,&info) &&
                !(info.dwFileAttributes&(FILE_ATTRIBUTE_DIRECTORY|FILE_ATTRIBUTE_REPARSE_POINT)) &&
                GetFileSizeEx(file,&size) && size.QuadPart>0 && size.QuadPart<=64*1024*1024;
        }
        if(!result)CloseAssetPins();
        return result;
    }
    void PublishRetired(const ULONGLONG now) noexcept
    {
        snapshot.levelId=0;snapshot.localComponentCount=0;snapshot.local={};snapshot.localAnimationQualified=0;
        snapshot.localFsmState=0xFFFFFFFFu;snapshot.remoteDisplay={};snapshot.mediaQualified=0;
        snapshot.phase=RuntimePhase::Stopped;snapshot.monotonicMilliseconds=now;lastPublished=now;
        Statistics();Publish(shared->snapshot,snapshot);
    }
    bool RetireVisualEpoch(const bool cohortBoundary=false,const bool playingExit=false) noexcept
    {
        if(actor.Created() || actor.Poisoned() || snapshot.epochRetired || snapshot.sceneRetirements==0xFFFFFFFFu)return false;
        retiredPlayingState=scenePlayingState;retiredFlow=sceneFlow;retiredAtWorld=snapshot.worldUpdates;
        sceneRearmEligible=SceneFollowing() && cohortBoundary;retiredPlayingExitSeen=playingExit;
        if(GuestMode())guestIsolation.Retire();
        ++snapshot.sceneRetirements;
        snapshot.epochRetired=1;snapshot.remoteEntity=0;snapshot.levelId=0;snapshot.localComponentCount=0;
        snapshot.local={};snapshot.localAnimationQualified=0;snapshot.localFsmState=0xFFFFFFFFu;
        snapshot.remoteDisplay={};snapshot.mediaQualified=0;
        actorProfile=actorAnimationManager=0;
        sceneProfile=sceneAnimationManager=sceneRecord=sceneRoot=scenePlayingState=0;
        sceneFlow=sceneSceneManager=sceneLevelId=0;
        qualifiedSceneSeen=false;remoteInterpolation.Clear();lastDisplayTime=0;
        CloseAssetPins();
        PublishRetired(GetTickCount64());
        OutputDebugStringA("WinxCoop visual scene retired; owned resources cleaned before any new scene arm");
        if(SceneFollowing())OutputDebugStringA(!cohortBoundary?
            "WinxCoop scene rearm forbidden: owner changed without the captured native unload boundary":
            playingExit?"WinxCoop scene rearm awaits later quiescent Playing after witnessed original Exit":
                "WinxCoop scene rearm awaits captured original Playing Exit after profile or animation cleanup");
        return true;
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
    bool ApplyGuestStartup(CONTEXT& context) noexcept
    {
        // The original App Init has just returned from LoadSettings, before
        // AppHelper.Initialize and before the Logo's native startup consumer.
        // Change only this child's heap launch option, never its installed INI.
        BYTE live[16]{};std::uint32_t app{},table{},secondary{},oldLevel{},after{};BYTE buildPck{};
        const auto fail=[](const char* reason) noexcept {guestStartupFailure=reason;return false;};
        if(!guestStartupInstalled || guestStartupApplied || !configuration.startupLevel || configuration.startupLevel>50 ||
            configuration.startupEntry || context.Eip!=GuestSettingsReturn)
            return fail("startup identity or one-shot boundary differs");
        if(!owner || WaitForSingleObject(owner,0)!=WAIT_TIMEOUT)return fail("startup owner is no longer active");
        if(
            !Read(GuestSettingsReturn,live,sizeof(live)) || live[0]!=0xCC ||
            !Same(live+1,guestStartupSignature+1,sizeof(live)-1))return fail("startup instruction no longer matches its pin");
        if(!Read(0x755290,&app,4) || app<0x10000 || (app&3) || app>0xFFFFFF9B ||
            !Read(app,&table,4) || table!=0x700728 || !Read(app+0x10,&secondary,4) || secondary!=0x700724)
            return fail("initialized AppHelper physical identity differs");
        if(!Read(app+0x4B,&buildPck,1) || buildPck || !Read(app+0x60,&oldLevel,4) || oldLevel>50)
            return fail("native settings are not the supported ordinary startup mode");
        MEMORY_BASIC_INFORMATION region{};
        if(!VirtualQuery(reinterpret_cast<const void*>(app+0x60),&region,sizeof(region)) || region.State!=MEM_COMMIT ||
            region.Type!=MEM_PRIVATE || (region.Protect&PAGE_GUARD) || (region.Protect&0xFF)!=PAGE_READWRITE ||
            static_cast<std::uint64_t>(app)+0x64>reinterpret_cast<std::uintptr_t>(region.BaseAddress)+region.RegionSize)
            return fail("startup option is outside writable private AppHelper storage");
        SIZE_T wrote{};
        if(!WriteProcessMemory(GetCurrentProcess(),reinterpret_cast<void*>(app+0x60),&configuration.startupLevel,4,&wrote) || wrote!=4 ||
            !Read(app+0x60,&after,4) || after!=configuration.startupLevel || Word(0x755290)!=app || Word(app)!=table)
            return fail("startup option write or identity readback differs");
        guestStartupApplied=true;
        // Exact original A1 [755290] semantics: only EAX and EIP change. The
        // OS restores all remaining integer/FP/SSE state from this context.
        context.Eax=app;context.Eip=GuestSettingsReturn+5;
        char text[]="WinxCoop guest startup applied level=00000000 entry=00000000 iniUnchanged=true";
        const auto at=sizeof("WinxCoop guest startup applied level=")-1;
        for(unsigned i=0;i<8;++i)text[at+i]="0123456789ABCDEF"[(configuration.startupLevel>>(28-i*4))&15];
        OutputDebugStringA(text);
        return true;
    }
    void ObserveAnimation(const std::uint32_t record) noexcept
    {
        snapshot.localAnimationQualified=0;snapshot.localFsmState=0xFFFFFFFFu;
        const auto head=Word(record+0x1C);
        if(head<0x10000)return;
        std::uint32_t previous=head,link=Word(head),character{},bloom{},move{},seen[11]{};
        for(unsigned index=0;index<11;++index)
        {
            std::uint32_t entry[3]{};
            if(link<0x10000 || link==head || !Read(link,entry,sizeof(entry)) || entry[1]!=previous || entry[2]<0x10000)return;
            for(unsigned old=0;old<index;++old)if(seen[old]==link)return;
            seen[index]=link;
            const auto table=Word(entry[2]);
            if(table==0x6F6438){if(character || Word(entry[2]+0x24)!=record)return;character=entry[2];}
            if(table==0x6FA288){if(bloom || Word(entry[2]+0x24)!=record)return;bloom=entry[2];}
            if(table==0x6F64F8){if(move || Word(entry[2]+0x24)!=record)return;move=entry[2];}
            previous=link;link=entry[0];
        }
        const auto input=Word(0x7552A0),command=Word(input+0x18);
        if(link!=head || Word(head+4)!=previous || !character || !bloom || !move || Word(input)!=0x6F5508 || Word(command)!=0x703090 ||
            Word(character+0x124)!=bloom || Word(character+0x12C)!=move || Word(character+0x130)!=command ||
            Word(bloom+0x124)!=character || Word(bloom+0x12C)!=command || Word(move+0x160)!=character)return;
        std::uint32_t state{};float magnitude{};
        if(!Read(bloom+0x130,&state,4) || !Read(command+4,&magnitude,4) || !Finite(magnitude) || magnitude<0 || magnitude>1)return;
        snapshot.localFsmState=state;
        if(state!=0 && state!=18)return;
        snapshot.localAnimation=state==18?RuntimeAnimation::Jump:magnitude>0.01f?RuntimeAnimation::Walk:RuntimeAnimation::Idle;
        snapshot.localAnimationQualified=1;
    }
    bool Observe() noexcept
    {
        observationReason=0;
        snapshot.localAnimationQualified=0;snapshot.localFsmState=0xFFFFFFFFu;
        snapshot.levelId=0;snapshot.localComponentCount=0;snapshot.local={};
        const auto flow = Word(0x755294), sceneManager = Word(0x75DB90);
        std::uint32_t level{}, pending{}, stack{}, currentScene{};
        if (flow < 0x10000 || Word(flow)!=0x702D2C || sceneManager < 0x10000 ||
            !Read(flow + 0x1B0,&level,4) || !Read(flow + 0x1B8,&pending,4) ||
            !Read(flow + 0x1AC,&stack,4) || !Read(sceneManager + 0x20,&currentScene,4)) {observationReason=1;return false;}
        const auto active = stack <= 20 ? Word(flow + 0x15C + stack * 4) : 0;
        // Ordinary boot/menu/cinematic flows are an expected wait. Do not even
        // follow the local player chain until the current native Playing is typed.
        // currentScene==0 is the WorldRET quiescence marker, not a level ID.
        if(Word(active)!=0x710874 || level<1 || level>50 || pending || Word(active+0x10)!=level || currentScene)
        {observationReason=2;return false;}
        const auto profile = Word(0x765AD4), record = Word(profile + 0x2B4), root = Word(record + 0x24);
        if (Word(profile) != 0x6F5A44 || Word(record) != 0x710F80 || Word(root) != 0x6DC4F4 ||
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
        ObserveAnimation(record);
        return true;
    }
    bool SameCommand(const RuntimeCommand& a, const RuntimeCommand& b) noexcept
    {
        return a.bytes==b.bytes && a.version==b.version && a.session==b.session && a.levelEpoch==b.levelEpoch &&
            a.sequence==b.sequence && a.entity==b.entity && a.operation==b.operation && a.animation==b.animation &&
            a.sceneGeneration==b.sceneGeneration && a.targetLevel==b.targetLevel && a.targetEntry==b.targetEntry &&
            Same(&a.remote,&b.remote,sizeof(a.remote));
    }
    bool CommandTransform(const Transform& transform) noexcept
    {
        for(float x:transform.position)if(!Finite(x) || x < -1000000 || x > 1000000)return false;
        for(float x:transform.scale)if(!Finite(x) || x<=0)return false;
        for(float x:transform.orientation)if(!Finite(x))return false;
        return true; // PassiveActor independently qualifies matrix and stock scale.
    }
    bool MaintenanceUi(const std::uint32_t object) noexcept
    {
        struct Ui final { std::uint32_t table,id,getter,record; };
        static constexpr Ui allowed[]={
            {0x710398,53,0x5F0080,0x768A50}, // Pause
            {0x70ED30,61,0x5D53C0,0x767FD8}, // Diary
            {0x70FD38,66,0x5E5310,0x768750}}; // Options/exit confirmation
        std::uint32_t table{},id{},getter{};
        if(object<0x10000 || !Read(object,&table,4) || !Read(object+0x10,&id,4))return false;
        for(const auto& ui:allowed)if(table==ui.table && id==ui.id)
        {
            BYTE code[]={0xB8,static_cast<BYTE>(ui.record),static_cast<BYTE>(ui.record>>8),
                static_cast<BYTE>(ui.record>>16),static_cast<BYTE>(ui.record>>24),0xC3};
            return Word(sceneFlow+0x18+ui.id*4)==object && Read(table+0x10,&getter,4) && getter==ui.getter &&
                pinnedImage.VerifyFeatureCode(ui.getter,code,sizeof(code));
        }
        return false;
    }
    bool RetainedForMaintenance() noexcept
    {
        // Tick has independently validated the live actor owner. This narrower
        // admission is only the captured Playing underneath known stock UI,
        // with no queued flow operation and the original world quiescence.
        // It cannot create a visual, change scenes or revive a retired epoch.
        if(!ActorPhase() || !qualifiedSceneSeen || snapshot.epochRetired || !actor.Created() || actor.Poisoned() ||
            Word(0x755294)!=sceneFlow || Word(sceneFlow)!=0x702D2C || Word(0x75DB90)!=sceneSceneManager ||
            Word(0x765AD4)!=sceneProfile || Word(sceneProfile)!=0x6F5A44 || Word(sceneProfile+0x2B4)!=sceneRecord ||
            Word(sceneRecord)!=0x710F80 || Word(sceneRecord+0x20)!=11 || Word(sceneRecord+0x24)!=sceneRoot ||
            Word(sceneRoot)!=0x6DC4F4 || Word(Word(sceneRoot+0x3C))!=0x6E7358 ||
            Word(0x765ADC)!=sceneAnimationManager || Word(sceneAnimationManager)!=0x703470 ||
            Word(scenePlayingState)!=0x710874 || Word(scenePlayingState+0x10)!=sceneLevelId ||
            sceneLevelId<1 || sceneLevelId>50)return false;
        std::uint32_t depth{},pending{},currentScene{},current{};
        if(!Read(sceneFlow+0x1AC,&depth,4) || !depth || depth>20 ||
            !Read(sceneFlow+0x1B8,&pending,4) || pending ||
            !Read(sceneSceneManager+0x20,&currentScene,4) || currentScene ||
            !Read(sceneFlow+0x1B0,&current,4))return false;
        // All ten queue records must be empty; a consumed first slot does
        // not independently rule out a later delayed operation.
        for(unsigned i=1;i<10;++i)
            if(!Read(sceneFlow+0x1B8+i*12,&pending,4) || pending)return false;
        bool found=false;unsigned above{};
        for(std::uint32_t i=0;i<=depth;++i)
        {
            std::uint32_t object{};if(!Read(sceneFlow+0x15C+i*4,&object,4))return false;
            if(object==scenePlayingState){if(found)return false;found=true;}
            else if(found){if(!MaintenanceUi(object))return false;++above;}
            if(i==depth && (!found || !above || Word(object+0x10)!=current))return false;
        }
        return found && above;
    }
    void TraceMaintenance(const RuntimeCommand& command,const std::uint32_t nativeBefore) noexcept
    {
        char text[]="WinxCoop retained-ui maintenance ACK operation=00000000 sequence=00000000 nativeBefore=00000000 nativeAfter=00000000";
        const std::uint32_t values[]={static_cast<std::uint32_t>(command.operation),command.sequence,nativeBefore,actor.Statistics().nativeCalls};
        constexpr unsigned offsets[]={
            sizeof("WinxCoop retained-ui maintenance ACK operation=")-1,
            sizeof("WinxCoop retained-ui maintenance ACK operation=00000000 sequence=")-1,
            sizeof("WinxCoop retained-ui maintenance ACK operation=00000000 sequence=00000000 nativeBefore=")-1,
            sizeof("WinxCoop retained-ui maintenance ACK operation=00000000 sequence=00000000 nativeBefore=00000000 nativeAfter=")-1};
        static_assert(offsets[3]+8==sizeof(text)-1,"bounded maintenance trace fields");
        for(unsigned i=0;i<4;++i)for(unsigned j=0;j<8;++j)text[offsets[i]+j]="0123456789ABCDEF"[(values[i]>>(28-j*4))&15];
        OutputDebugStringA(text);
    }
    void CommandOutcome(const RuntimeCommand& command,const RuntimeAckStatus status,const RuntimeError error) noexcept
    {
        Statistics();lastCommand=command;snapshot.lastCommandSequence=command.sequence;
        lastAck={};lastAck.bytes=sizeof(lastAck);lastAck.version=IpcVersion;lastAck.session=command.session;
        lastAck.levelEpoch=command.levelEpoch;lastAck.sequence=command.sequence;lastAck.entity=command.entity;
        lastAck.operation=command.operation;lastAck.status=status;lastAck.error=error;
        lastAck.worldUpdates=snapshot.worldUpdates;lastAck.actorError=snapshot.actorError;lastAck.sceneGeneration=command.sceneGeneration;
        Publish(shared->acknowledgement,lastAck);
        // The actual consumed outcome and its witness must be visible before
        // returning to original code, including an immediate stock Quit.
        // This also covers the private Follow completion outside a World Tick.
        snapshot.monotonicMilliseconds=GetTickCount64();lastPublished=snapshot.monotonicMilliseconds;
        Publish(shared->snapshot,snapshot);
    }
    bool Commands(const bool ready,const bool retainedForMaintenance,CONTEXT& context,bool& redirected) noexcept
    {
        RuntimeCommand command{};
        if(!TryRead(shared->command,command))return true;
        if(!command.sequence)
        {
            RuntimeCommand empty{};
            return SameCommand(command,empty) && !lastCommand.sequence;
        }
        const Transform emptyTransform{};
        const bool canonicalFollow=command.operation==RuntimeOperation::FollowLevel && SceneFollowing() && GuestMode() &&
            command.entity==0 && command.animation==RuntimeAnimation::Idle && command.targetLevel>=1 && command.targetLevel<=50 &&
            command.targetEntry==0 && Same(&command.remote,&emptyTransform,sizeof(emptyTransform));
        const bool canonicalVisual=command.operation>=RuntimeOperation::Create && command.operation<=RuntimeOperation::Remove &&
            command.entity>=1 && command.entity<=2 && !command.targetLevel && !command.targetEntry && command.animation<=RuntimeAnimation::Jump;
        if(command.bytes!=sizeof(command) || command.version!=IpcVersion || command.session!=configuration.session ||
            command.levelEpoch!=configuration.levelEpoch || !command.sceneGeneration || command.sceneGeneration>snapshot.sceneGeneration ||
            (!canonicalFollow && !canonicalVisual) || !PassiveMode())return false;
        if(followPending)return SameCommand(command,pendingFollow) && guestTransition.Pending();
        if(command.sequence==lastCommand.sequence)
        {
            if(!SameCommand(command,lastCommand))return false;
            return true; // Already acknowledged, never execute twice.
        }
        if(lastCommand.sequence==0xFFFFFFFF || command.sequence!=lastCommand.sequence+1)return false;
        RuntimeAck previous{};
        if(!TryRead(shared->acknowledgement,previous) || !Same(&previous,&lastAck,sizeof(lastAck)))return false;
        if(SceneFollowing() && (snapshot.epochRetired || command.sceneGeneration<snapshot.sceneGeneration))
        {
            if(command.sceneGeneration>snapshot.sceneRetirements)return false;
            CommandOutcome(command,RuntimeAckStatus::CancelledOnRetirement,RuntimeError::None);
            OutputDebugStringA("WinxCoop immutable command consumed as CancelledOnRetirement; no native operation executed");
            return true;
        }
        if(snapshot.epochRetired || command.sceneGeneration!=snapshot.sceneGeneration)return false;
        if(command.operation==RuntimeOperation::FollowLevel)
        {
            if(!ready || !guestTransition.CanBeginAtWorld(context,command.targetLevel,command.targetEntry))return true;
            if(actor.Created() || snapshot.remoteEntity || snapshot.remoteCreated!=snapshot.remoteRemoved ||
                !guestTransition.BeginAtWorld(context,command.targetLevel,command.targetEntry))return false;
            pendingFollow=command;followPending=true;redirected=true;return true;
        }
        // A pending Apply may admit only its interpolation target while exact
        // retained UI covers Playing. Remove may clean that retained actor.
        // Create and all unknown/not-ready contexts still hold unchanged.
        const bool maintenance=!ready && retainedForMaintenance;
        if(!ready && (!maintenance || command.operation==RuntimeOperation::Create))return true;
        const auto nativeBefore=actor.Statistics().nativeCalls;
        bool applied=false;
        if(command.operation==RuntimeOperation::Create)
        {
            if(actor.Created() || snapshot.remoteEntity || !CommandTransform(command.remote) || !MediaMatchesNative() || !PinAssets())return false;
            actorProfile=Word(0x765AD4);actorAnimationManager=Word(0x765ADC);
            if(actorProfile<0x10000 || Word(actorAnimationManager)!=0x703470)return false;
            remoteInterpolation.Clear();
            applied=actor.Create(configuration.mediaDirectory) && actor.ApplyRemote(command.remote,static_cast<PassiveAnimation>(command.animation)) &&
                remoteInterpolation.Reset(command.remote,GetTickCount64());
            if(applied)
            {
                snapshot.remoteEntity=command.entity;remoteAnimation=command.animation;snapshot.remoteDisplay=command.remote;
                snapshot.remoteDisplaySamples=0;snapshot.remoteDisplayMaxGapMs=0;snapshot.remoteDisplayMaxStep=0;
                lastDisplayTime=GetTickCount64();
            }
        }
        else
        {
            if(!actor.Created() || snapshot.remoteEntity!=command.entity || Word(0x765AD4)!=actorProfile ||
                Word(0x765ADC)!=actorAnimationManager || !MediaMatchesNative())return false;
            if(command.operation==RuntimeOperation::Apply)
            {
                Transform current{};const auto now=GetTickCount64();
                applied=CommandTransform(command.remote) && remoteInterpolation.SetTarget(command.remote,now) && remoteInterpolation.Sample(now,current) &&
                    (maintenance || actor.ApplyRemote(current,static_cast<PassiveAnimation>(command.animation)));
                if(applied)remoteAnimation=command.animation;
            }
            else
            {
                applied=actor.DestroyBeforeUnload() && !actor.Created();
                if(applied){snapshot.remoteEntity=0;actorProfile=actorAnimationManager=0;remoteInterpolation.Clear();lastDisplayTime=0;}
            }
        }
        CommandOutcome(command,applied?RuntimeAckStatus::Applied:RuntimeAckStatus::Failed,applied?RuntimeError::None:RuntimeError::Actor);
        if(applied && maintenance)TraceMaintenance(command,nativeBefore);
        if(!applied)FailClosed(RuntimeError::Actor);
        return applied;
    }
    bool RearmPins() noexcept
    {
        // Source was verified and file-pinned once before publication. Preserve
        // those pins and all global hooks; recheck every admitted live feature
        // before observing a new cohort instead of installing hooks again.
        for(const auto& guard:unloadGuards)if(!guard.installed || !GuardSignature(guard))return false;
        for(const auto& pin:image_compatibility::CodePins)
        {
            bool own=pin.address==WorldReturn;
            for(const auto& guard:unloadGuards)if(pin.address==guard.address)own=true;
            if(!own && !pinnedImage.Verify(pin.address,pin.size))return false;
        }
        for(const auto& pin:image_compatibility::LayoutPins)
        {
            BYTE bytes[32]{};
            if(pin.size>sizeof(bytes) || !Read(pin.address,bytes,pin.size) || !Same(bytes,pin.bytes.data(),pin.size))return false;
        }
        return true;
    }
    bool EmptyFlowQueue() noexcept
    {
        const auto flow=Word(0x755294);if(Word(flow)!=0x702D2C)return false;
        for(unsigned i=0;i<10;++i){std::uint32_t target{};if(!Read(flow+0x1B8+i*12,&target,4) || target)return false;}
        return true;
    }
    void Tick(CONTEXT& context,bool& redirected) noexcept
    {
        ++snapshot.worldUpdates;
        if(!owner || WaitForSingleObject(owner,0)!=WAIT_TIMEOUT || shared->magic!=IpcMagic || shared->version!=IpcVersion ||
            shared->bytes!=sizeof(*shared) || shared->nonce!=configuration.nonce || shared->launcherPid!=GetProcessId(owner))
            {FailClosed(RuntimeError::Owner);return;}
        if(GuestMode() && SceneFollowing() && !guestTransition.OnLaterWorld(context))
            {TraceDetail("WinxCoop guest transition cleanup failure: ",guestTransition.Failure());FailClosed(RuntimeError::NativeException);return;}
        // Only an explicitly selected menu-first service may arm a new native
        // cohort. A real old Playing Exit must have been witnessed, including
        // when an earlier profile/animation clear retired its visual first.
        if(snapshot.epochRetired)
        {
            if(SceneFollowing() && !Commands(false,false,context,redirected)){FailClosed(RuntimeError::Command);return;}
            if(!SceneFollowing() || !sceneRearmEligible || !retiredPlayingExitSeen || snapshot.worldUpdates<=retiredAtWorld ||
                !Observe() || !EmptyFlowQueue() || !MediaMatchesNative())
            {const auto publishNow=GetTickCount64();if(publishNow-lastPublished>=50)PublishRetired(publishNow);return;}
            if(!RearmPins() || actor.Created() || actor.Poisoned() || snapshot.remoteEntity || snapshot.remoteCreated!=snapshot.remoteRemoved ||
                snapshot.sceneGeneration==0xFFFFFFFFu){FailClosed(RuntimeError::Unload);return;}
            if(GuestMode())
            {
                const auto target=guestTransition.TargetLevel();
                if(!target || snapshot.levelId!=target)
                {const auto publishNow=GetTickCount64();if(publishNow-lastPublished>=50)PublishRetired(publishNow);return;}
                if(!guestIsolation.BeginNextScene(target))
                {TraceDetail("WinxCoop guest scene rearm failure: ",guestIsolation.Failure());FailClosed(RuntimeError::Identity);return;}
            }
            ++snapshot.sceneGeneration;snapshot.epochRetired=0;sceneRearmEligible=false;retiredPlayingExitSeen=false;
            retiredPlayingState=retiredFlow=retiredAtWorld=0;
            OutputDebugStringA("WinxCoop new native scene generation armed after exact old Playing Exit; hooks and IPC sequence retained");
        }
        const auto timer=Word(0x755298);BYTE paused{},timerActive{};
        snapshot.gameTimerPaused=Read(timer+0x40,&paused,1)?paused:0xFFFFFFFFu;
        snapshot.gameTimerActive=Read(timer+0x18,&timerActive,1)?timerActive:0xFFFFFFFFu;
        if(!Read(timer+0x1C,&snapshot.gameTimerCurrentRaw,4))snapshot.gameTimerCurrentRaw=0;
        if(!Read(timer+0x28,&snapshot.gameTimerDeltaRaw,4))snapshot.gameTimerDeltaRaw=0;
        const bool ready=Observe();
        if(!snapshot.epochRetired)snapshot.phase=ready?RuntimePhase::Ready:RuntimePhase::WaitingForLevel;
        if(PassiveMode())
        {
            for(const auto& guard:unloadGuards)if(!guard.installed || !GuardSignature(guard)){FailClosed(RuntimeError::Signature);return;}
            if(ready && !snapshot.epochRetired)
            {
                const auto profile=Word(0x765AD4),record=Word(profile+0x2B4),root=Word(record+0x24),manager=Word(0x765ADC);
                const auto flow=Word(0x755294),stack=Word(flow+0x1AC),playing=stack<=20?Word(flow+0x15C+stack*4):0;
                if(Word(manager)!=0x703470 || Word(playing)!=0x710874){FailClosed(RuntimeError::Identity);return;}
                if(!qualifiedSceneSeen)
                {
                    if(configuration.startupLevel)
                    {
                        const auto expected=snapshot.sceneGeneration==1?configuration.startupLevel:guestTransition.TargetLevel();
                        if(!guestStartupApplied || snapshot.levelId!=expected)
                        {TraceDetail("WinxCoop guest startup failure: ","first actual Playing scene differs from assigned startup");FailClosed(RuntimeError::Identity);return;}
                        std::uint32_t slot=0xFFFFFFFFu;const bool slotRead=Read(profile+0x6DC,&slot,4);
                        char text[]="WinxCoop guest startup ready level=00000000 saveSlotRaw=00000000 slotRead=0";
                        const auto levelAt=sizeof("WinxCoop guest startup ready level=")-1;
                        const auto slotAt=sizeof("WinxCoop guest startup ready level=00000000 saveSlotRaw=")-1;
                        for(unsigned i=0;i<8;++i)
                        {
                            text[levelAt+i]="0123456789ABCDEF"[(snapshot.levelId>>(28-i*4))&15];
                            text[slotAt+i]="0123456789ABCDEF"[(slot>>(28-i*4))&15];
                        }
                        text[sizeof(text)-2]=slotRead?'1':'0';OutputDebugStringA(text);
                        if(!guestIsolation.BindMotion(record))
                        {TraceDetail("WinxCoop guest isolation failure: ",guestIsolation.Failure());FailClosed(RuntimeError::Identity);return;}
                        if(!guestIsolation.SceneReady())
                        {TraceDetail("WinxCoop guest flow readiness failure: ",guestIsolation.Failure());FailClosed(RuntimeError::Identity);return;}
                    }
                    sceneProfile=profile;sceneAnimationManager=manager;sceneRecord=record;sceneRoot=root;scenePlayingState=playing;
                    sceneFlow=flow;sceneSceneManager=Word(0x75DB90);sceneLevelId=snapshot.levelId;
                    qualifiedSceneSeen=true;
                }
                else if(sceneProfile!=profile || sceneAnimationManager!=manager || sceneRecord!=record || sceneRoot!=root || scenePlayingState!=playing)
                {
                    // With no visual there are no native resources to remove.
                    // Retire an observed old cohort without touching its objects.
                    if(!actor.Created() && RetireVisualEpoch())return;
                    TraceLifetimeFailure();FailClosed(RuntimeError::Unload);return;
                }
                if(GuestMode() && !guestIsolation.VerifyMotion(record))
                {TraceDetail("WinxCoop guest isolation failure: ",guestIsolation.Failure());FailClosed(RuntimeError::Identity);return;}
                if(GuestMode() && !guestIsolation.CompletePresentation())
                {TraceDetail("WinxCoop guest presentation failure: ",guestIsolation.Failure());FailClosed(RuntimeError::Identity);return;}
            }
            const bool mediaMatches=MediaMatchesNative();
            snapshot.mediaQualified=mediaMatches?1:0;
            if(actor.Created())
            {
                // Command readiness is narrower than lifetime. A flow overlay
                // may pause an intact scene before any native unload entry.
                const bool owned=actor.ValidateLiveOwnership();
                if(!owned || snapshot.epochRetired || !mediaMatches || Word(0x765AD4)!=actorProfile || Word(0x765ADC)!=actorAnimationManager)
                    {TraceLifetimeFailure();FailClosed(RuntimeError::Unload);return;}
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
            if(!Commands(ready && !snapshot.epochRetired,!ready && mediaMatches && RetainedForMaintenance(),context,redirected)){FailClosed(RuntimeError::Command);return;}
            if(redirected)return;
            if(ready && actor.Created())
            {
                const auto renderNow=GetTickCount64();
                Transform display{};
                if(!remoteInterpolation.Sample(renderNow,display)){FailClosed(RuntimeError::Actor);return;}
                if(!Same(&display,&snapshot.remoteDisplay,sizeof(display)) && !actor.ApplyRemote(display,static_cast<PassiveAnimation>(remoteAnimation)))
                    {FailClosed(RuntimeError::Actor);return;}
                double distanceSquared{};
                for(unsigned axis=0;axis<3;++axis)
                {const double step=static_cast<double>(display.position[axis])-snapshot.remoteDisplay.position[axis];distanceSquared+=step*step;}
                const auto distance=static_cast<float>(std::sqrt(distanceSquared));
                if(distance>snapshot.remoteDisplayMaxStep)snapshot.remoteDisplayMaxStep=distance;
                const auto gap=lastDisplayTime && renderNow>=lastDisplayTime?renderNow-lastDisplayTime:0;
                if(gap>snapshot.remoteDisplayMaxGapMs)snapshot.remoteDisplayMaxGapMs=gap>0xFFFFFFFFu?0xFFFFFFFFu:static_cast<std::uint32_t>(gap);
                snapshot.remoteDisplay=display;++snapshot.remoteDisplaySamples;lastDisplayTime=renderNow;
            }
            else if(!ready)lastDisplayTime=0;
        }
        else
        {
            RuntimeCommand empty{},command{};
            if(TryRead(shared->command,command) && !SameCommand(command,empty)){FailClosed(RuntimeError::Command);return;}
        }
        if(InterlockedCompareExchange(&terminalError,0,0))return;
        const auto publishNow=GetTickCount64();
        if (publishNow - lastPublished < 50) return;lastPublished = publishNow;
        snapshot.monotonicMilliseconds = publishNow;
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
        const auto exactPlayingExit=[&](const std::uint32_t flow,const std::uint32_t playing) noexcept
        {
            std::uint32_t depth{};
            return guard.address==0x5F86C0 && playing && context.Ecx==playing && Word(playing)==0x710874 &&
                Word(0x755294)==flow && Word(flow)==0x702D2C && Read(flow+0x1AC,&depth,4) && depth<=20 &&
                Word(flow+0x15C+depth*4)==playing;
        };
        if(snapshot.epochRetired && sceneRearmEligible && !retiredPlayingExitSeen &&
            exactPlayingExit(retiredFlow,retiredPlayingState))
        {
            retiredPlayingExitSeen=true;
            OutputDebugStringA("WinxCoop retired cohort reached its exact original Playing Exit; later scene observation may rearm");
        }
        bool cohortBoundary=false;
        if(qualifiedSceneSeen && !snapshot.epochRetired)
        {
            if(Word(0x765AD4)!=sceneProfile || Word(0x765ADC)!=sceneAnimationManager)
            {
                if(actor.Created() || !RetireVisualEpoch())return false;
            }
            else
            {
                cohortBoundary=((guard.address==0x4E8AA0 || guard.address==0x4E8C80) && context.Ecx==sceneProfile) ||
                    ((guard.address==0x59AE80 || guard.address==0x59B190) && context.Ecx==sceneAnimationManager) ||
                    (guard.address==0x5F86C0 && context.Ecx==scenePlayingState);
            }
        }
        // Other native objects may use the same methods. Their original PUSH
        // passes through; only OUR captured owner retires/removes this visual.
        if(cohortBoundary && actor.Created())
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
            remoteInterpolation.Clear();lastDisplayTime=0;
            OutputDebugStringA("WinxCoop passive actor removed before native unload entry");
        }
        // A profile/animation clear alone is not a completed scene switch.
        // A selected follow service additionally latches the exact Playing Exit
        // before a later quiescent world may observe and bind a new cohort.
        if(cohortBoundary && qualifiedSceneSeen)
        {
            if(!RetireVisualEpoch(true,exactPlayingExit(sceneFlow,scenePlayingState)))return false;
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
        const bool guestStartup=guestStartupInstalled && at==GuestSettingsReturn;
        const bool guestPolicy=GuestMode() && guestIsolation.Handles(static_cast<std::uint32_t>(at));
        const bool followCompletion=GuestMode() && SceneFollowing() && guestTransition.Handles(static_cast<std::uint32_t>(at));
        if(at!=WorldReturn && !guard && !guestStartup && !guestPolicy && !followCompletion)return EXCEPTION_CONTINUE_SEARCH;
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
        if(followCompletion)
        {
            RuntimeCommand current{};RuntimeAck previous{};bool completed=false;
            __try
            {
                completed=followPending && TryRead(shared->command,current) && SameCommand(current,pendingFollow) &&
                    TryRead(shared->acknowledgement,previous) && Same(&previous,&lastAck,sizeof(lastAck)) &&
                    guestTransition.Complete(*exception->ContextRecord);
            }
            __except(EXCEPTION_EXECUTE_HANDLER) { FailClosed(RuntimeError::NativeException); }
            if(!completed)
            {TraceDetail("WinxCoop guest follow completion failure: ",guestTransition.Failure());FailClosed(RuntimeError::Command);return EXCEPTION_CONTINUE_SEARCH;}
            snapshot.phase=RuntimePhase::WaitingForLevel;snapshot.levelId=0;snapshot.localComponentCount=0;
            snapshot.local={};snapshot.localAnimationQualified=0;snapshot.localFsmState=0xFFFFFFFFu;
            CommandOutcome(pendingFollow,RuntimeAckStatus::Applied,RuntimeError::None);pendingFollow={};followPending=false;
            OutputDebugStringA("WinxCoop FollowLevel acknowledged after original native flow request completed; awaiting actual scene readiness");
            InterlockedExchange(&inHandler,0);return EXCEPTION_CONTINUE_EXECUTION;
        }
        if(guestPolicy)
        {
            bool completed=false;
            __try { completed=guestIsolation.Handle(*exception->ContextRecord); }
            __except(EXCEPTION_EXECUTE_HANDLER) { FailClosed(RuntimeError::NativeException); }
            if(!completed)
            {TraceDetail("WinxCoop guest isolation failure: ",guestIsolation.Failure());FailClosed(RuntimeError::Command);return EXCEPTION_CONTINUE_SEARCH;}
            InterlockedExchange(&inHandler,0);return EXCEPTION_CONTINUE_EXECUTION;
        }
        if(guestStartup)
        {
            bool completed=false;
            __try { completed=ApplyGuestStartup(*exception->ContextRecord); }
            __except(EXCEPTION_EXECUTE_HANDLER) { FailClosed(RuntimeError::NativeException); }
            if(!completed){TraceDetail("WinxCoop guest startup failure: ",guestStartupFailure);FailClosed(RuntimeError::Identity);return EXCEPTION_CONTINUE_SEARCH;}
            InterlockedExchange(&inHandler,0);return EXCEPTION_CONTINUE_EXECUTION;
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
        bool redirected=false;
        __try { Tick(*exception->ContextRecord,redirected); }
        __except(EXCEPTION_EXECUTE_HANDLER) { FailClosed(RuntimeError::NativeException); }
        safePhase=SafePhase::None;
        if(InterlockedCompareExchange(&terminalError,0,0))return EXCEPTION_CONTINUE_SEARCH;
        if(!redirected)
        {
            exception->ContextRecord->Eip = caller;
            exception->ContextRecord->Esp += 4; // Exact semantics of the verified C3.
        }
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
        start.mode>RuntimeMode::PassiveActor || start.startupLevel>50 || start.startupEntry || start.sceneFollow>1 ||
        (start.sceneFollow && start.mode!=RuntimeMode::PassiveActor) ||
        (start.mode==RuntimeMode::PassiveActor && (!start.session || !start.levelEpoch)) ||
        (start.mode==RuntimeMode::Observe && (start.session || start.levelEpoch || start.mediaDirectory[0] || start.startupLevel)))
        return static_cast<DWORD>(RuntimeError::Identity);
    configuration=start;
    if (!pinnedImage.Open())
    {
        // The owned debugger records bounded ASCII "WinxCoop " messages.
        // Failure contains static feature names/reasons, never user paths.
        char text[384]{};char* cursor=text;char* const end=text+sizeof(text)-1;
        const auto append=[&cursor,end](const char* value) noexcept
        {while(value && *value && cursor<end)*cursor++=*value++;};
        append("WinxCoop image feature gate failed address=");
        const auto address=pinnedImage.FailureAddress();
        for(unsigned i=0;i<8 && cursor<end;++i)*cursor++="0123456789ABCDEF"[(address>>(28-i*4))&15];
        append(" reason=");append(pinnedImage.Failure());*cursor=0;
        OutputDebugStringA(text);
        return static_cast<DWORD>(RuntimeError::Image);
    }
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
    if(start.startupLevel)
    {
        if(!pinnedImage.VerifyGuestStartup())
        {TraceDetail("WinxCoop guest startup feature gate failed: ",pinnedImage.Failure());CloseResourcesBeforeHooks();return static_cast<DWORD>(RuntimeError::Signature);}
        if(!Read(GuestSettingsReturn,guestStartupSignature,sizeof(guestStartupSignature)))
        {TraceDetail("WinxCoop guest startup failure: ","settings-return signature read failed");CloseResourcesBeforeHooks();return static_cast<DWORD>(RuntimeError::Signature);}
        if(!guestIsolation.Prepare(pinnedImage,mainThread,start.startupLevel))
        {TraceDetail("WinxCoop guest isolation preparation failed: ",guestIsolation.Failure());CloseResourcesBeforeHooks();return static_cast<DWORD>(RuntimeError::Signature);}
        if(start.sceneFollow && !guestTransition.Prepare(pinnedImage,guestIsolation,mainThread))
        {TraceDetail("WinxCoop guest transition preparation failed: ",guestTransition.Failure());CloseResourcesBeforeHooks();return static_cast<DWORD>(RuntimeError::Signature);}
    }
    snapshot.gamePid = start.gamePid; snapshot.mainThreadId = mainThread; snapshot.phase = RuntimePhase::Attached;
    snapshot.mode=start.mode;snapshot.session=start.session;snapshot.levelEpoch=start.levelEpoch;
    snapshot.sceneGeneration=1;
    if(PassiveMode())
    {
        // Ordinary boot/menu does not load or content-hash any visual asset.
        // The explicit Create path pins files and validates their native graph.
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
    if(start.startupLevel)
    {
        if(!InstallByte(GuestSettingsReturn)){FailClosed(RuntimeError::Signature);return static_cast<DWORD>(RuntimeError::Signature);}
        guestStartupInstalled=true;
    }
    if(PassiveMode())for(auto& guard:unloadGuards)
    {
        if(!InstallByte(guard.address)){FailClosed(RuntimeError::Signature);return static_cast<DWORD>(RuntimeError::Signature);}
        guard.installed=true;
    }
    if(start.startupLevel && !guestIsolation.Install())
    {TraceDetail("WinxCoop guest isolation publication failed: ",guestIsolation.Failure());FailClosed(RuntimeError::Signature);return static_cast<DWORD>(RuntimeError::Signature);}
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
