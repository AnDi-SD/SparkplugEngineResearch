#include "ReplicationCoordinator.h"
#include <bcrypt.h>
#include <algorithm>
#include <array>
#include <cmath>
#include <deque>
#include <limits>
#include <optional>
#include <stdexcept>
#include <utility>

namespace winxcoop::launcher
{
    using namespace networking;
    namespace
    {
        constexpr std::uint32_t MinimumHelperLimitSeconds=30, GameLimitGraceSeconds=15;
        constexpr std::uint64_t RemoveWaitMilliseconds=10000;
        constexpr bool CurrentHostObservation(const bool hasSnapshot,const bool ready,
            const std::uint64_t observedAt,const std::uint64_t now) noexcept
        {
            return hasSnapshot && ready && now>=observedAt && now-observedAt<=1000;
        }
        // This is the admission predicate used by Decide, not a native/game
        // fixture. A cinematic may suspend World publication with no error.
        static_assert(CurrentHostObservation(true,true,100,1100),"A fresh Ready observation can qualify");
        static_assert(!CurrentHostObservation(true,true,100,1101),"Stale Ready must become Busy, not acceptance");
        static_assert(!CurrentHostObservation(true,false,100,100),"A fresh pause/cinematic is Busy");
        static_assert(!CurrentHostObservation(true,false,100,20000),"A long cinematic is still Busy");
        static_assert(!CurrentHostObservation(false,true,100,100),"No native snapshot is Busy");
        static_assert(!CurrentHostObservation(true,true,101,100),"A future observation cannot qualify");
        void Require(const bool value,const char* error){if(!value)throw std::runtime_error(error);}
        std::uint64_t FreshId()
        {
            std::uint64_t value{};
            Require(BCryptGenRandom(nullptr,reinterpret_cast<PUCHAR>(&value),sizeof(value),BCRYPT_USE_SYSTEM_PREFERRED_RNG)>=0 && value,
                "Fresh private generation unavailable");return value;
        }
        std::wstring Wide(const std::string& value)
        {
            if(value.empty())return {};
            const auto count=MultiByteToWideChar(CP_UTF8,MB_ERR_INVALID_CHARS,value.data(),static_cast<int>(value.size()),nullptr,0);
            if(count<=0)return L"ошибка игрового адаптера";
            std::wstring result(static_cast<std::size_t>(count),L'\0');
            MultiByteToWideChar(CP_UTF8,MB_ERR_INVALID_CHARS,value.data(),static_cast<int>(value.size()),result.data(),count);return result;
        }
        std::uint32_t Next(std::uint32_t& value)
        {Require(value<std::numeric_limits<std::uint32_t>::max(),"Stage P state counter exhausted; a fresh connection is required");return ++value;}
        struct RemoteKey final
        {
            std::uint64_t connection{},session{};std::uint32_t epoch{},entity{};
            bool operator==(const RemoteKey& b) const noexcept
            {return connection==b.connection && session==b.session && epoch==b.epoch && entity==b.entity;}
        };
        struct Visual final {RemoteKey key;gameplay::AcceptedState state;std::uint64_t revision{};};
        struct NativePending final
        {RemoteKey key;adapter::RuntimeOperation operation{};std::uint32_t sequence{};std::uint64_t began{},revision{};std::uint32_t sceneGeneration{};};
        struct SceneAssignment final
        {
            RemoteKey key;gameplay::LevelDescriptor level;
            std::uint32_t fromNativeGeneration{};
            bool needsFollow{},requested{},requestAcknowledged{};
        };
        struct ObservationReceipt final {std::uint32_t sequence{};std::uint64_t began{};};
        struct ObservationDecision final {gameplay::ObservationToken token;bool withinNumericEnvelope{};};
        template<class State> bool NumericEnvelope(const State& state) noexcept
        {
            // Bounded native-scene safety only, not a movement/anti-cheat rule.
            for(const auto value:state.position)if(!std::isfinite(value) || std::abs(value)>1000000.0f)return false;
            for(const auto value:state.velocity)if(!std::isfinite(value) || std::abs(value)>1000000.0f)return false;
            return true;
        }

        // An original root matrix can accumulate small roundoff. Export a
        // finite right-handed orthonormal basis, retaining the native row order.
        // This is our wire normalization, not recovered movement/physics.
        bool Basis(const float* input,std::array<float,9>& output)
        {
            std::array<double,3> a{},b{},c{};
            for(unsigned i=0;i<3;++i)
            {if(!std::isfinite(input[i]) || !std::isfinite(input[i+3]) || !std::isfinite(input[i+6]))return false;a[i]=input[i];b[i]=input[i+3];}
            const auto dot=[](const auto& x,const auto& y){return x[0]*y[0]+x[1]*y[1]+x[2]*y[2];};
            const std::array<double,3> originalThird{{input[6],input[7],input[8]}};
            const auto determinant=a[0]*(b[1]*originalThird[2]-b[2]*originalThird[1])-a[1]*(b[0]*originalThird[2]-b[2]*originalThird[0])+a[2]*(b[0]*originalThird[1]-b[1]*originalThird[0]);
            if(std::abs(dot(a,a)-1)>0.01 || std::abs(dot(b,b)-1)>0.01 || std::abs(dot(originalThird,originalThird)-1)>0.01 ||
                std::abs(dot(a,b))>0.01 || std::abs(dot(a,originalThird))>0.01 || std::abs(dot(b,originalThird))>0.01 || std::abs(determinant-1)>0.01)return false;
            const auto normalize=[&](auto& x){const auto n=std::sqrt(dot(x,x));if(!std::isfinite(n) || n<0.5 || n>1.5)return false;for(auto& v:x)v/=n;return true;};
            if(!normalize(a))return false;const auto projection=dot(a,b);for(unsigned i=0;i<3;++i)b[i]-=projection*a[i];if(!normalize(b))return false;
            c={a[1]*b[2]-a[2]*b[1],a[2]*b[0]-a[0]*b[2],a[0]*b[1]-a[1]*b[0]};
            if(c[0]*input[6]+c[1]*input[7]+c[2]*input[8]<0.5)return false;
            for(unsigned i=0;i<3;++i){output[i]=static_cast<float>(a[i]);output[i+3]=static_cast<float>(b[i]);output[i+6]=static_cast<float>(c[i]);}return true;
        }
        bool Cleanup(const GameProcessClientResult& result) noexcept
        {
            if(result.directHost)
                return result.completed && result.directCleanupVerified;
            if(result.completed && !result.outerStarted)
                return !result.mediaLeaseInherited && (!result.mediaLeaseAcquired || result.mediaLeaseReleased);
            return result.completed && result.outerExited && result.outerExitCodeKnown && result.outerReportValid &&
                result.mediaRestoreAttempted && result.mediaRestoreVerified && result.mediaLeaseReleased && result.outerChildStopped &&
                !result.outerChildTimedOut && !result.outerTerminatedChild && result.helperReportValid && result.helperCompleted &&
                result.gameExitCodeKnown && result.gameExitEventContinued && result.gameCleanupConfirmed && !result.commandUnconfirmed;
        }
    }
    struct ReplicationCoordinator::Impl final
    {
        PrototypeProfile profile;
        WindowsTcpTransport* transport{};
        SessionRole role=SessionRole::Host,gameRole=SessionRole::Host;
        std::unique_ptr<gameplay::GameplayProtocol> protocol;
        std::unique_ptr<GameProcessClient> game;
        ReplicationMetrics metrics;
        GameProcessClientResult lastResult;
        std::string failure;
        UiStatus status{TextId::GameSelected};
        std::optional<RemoteKey> activeRemote;
        std::optional<Visual> desiredRemote;
        std::optional<NativePending> nativePending;
        std::optional<SceneAssignment> pendingAssignment;
        std::deque<ObservationDecision> decisions;
        std::deque<ObservationReceipt> receipts;
        adapter::Transform sceneTransform{},previousLocal{};
        std::uint64_t nextSample{},previousTime{},previousWorld{},visualRevision{},appliedRevision{},stopBegan{},lastSnapshotWorld{};
        std::uint64_t lastCooperativePump{};
        std::uint64_t gameSoftStopAt{};
        std::uint32_t lastNativeCreated{},lastNativeRemoved{},lastDisplaySamples{};
        std::uint32_t sourceTick{},hostOwnTick{},hostGuestTick{};
        std::uint32_t lastSceneGeneration{},lastSceneRetirements{};
        // An issued native load outlives a newer wire assignment. Its actual
        // retirement is expected even when the desired target is coalesced.
        std::uint32_t expectedFollowFromGeneration{};
        bool shutdown{},stopGameAfterRemoval{},stopRequested{},gameSoftLimitReached{},readyCounted{},haveSample{},haveScale{},hasResult{},allCompletedCleanup=true;
        bool sceneRetired{},guestSceneMismatch{},waitingForVisualRemoval{};
        bool guestSceneSuspended{};

        explicit Impl(PrototypeProfile value):profile(std::move(value))
        {Require(profile.filePins && profile.level.entryId==0 && (profile.paths.menuFirst || profile.level.levelId==28),"Missing selected installed-game/module ownership");}
        bool Active() const noexcept{return game && !game->Finished();}
        bool RetiredScene() const noexcept
        {return Active() && (sceneRetired || (game->HasRuntimeSnapshot() && game->Snapshot().epochRetired));}
        bool ReusableGuest() const noexcept
        {
            return profile.paths.menuFirst && Active() && gameRole==SessionRole::Guest &&
                readyCounted && haveScale && !expectedFollowFromGeneration && !RetiredScene() && !stopRequested && !stopGameAfterRemoval && game->Failure().empty();
        }
        bool Current(const gameplay::Event& event) const noexcept
        {
            // Scene controls remain FIFO even if a fast second assignment is
            // queued before the owner drains events. Old pose events are never
            // admitted by this exception.
            return protocol && event.session==protocol->SessionId() &&
                (event.epoch==protocol->LevelEpoch() ||
                 ((event.kind==gameplay::EventKind::SceneSuspended || event.kind==gameplay::EventKind::LevelAssigned) && event.epoch<protocol->LevelEpoch()));
        }
        void ClearSceneTraffic() noexcept
        {
            decisions.clear();receipts.clear();desiredRemote.reset();
            sourceTick=hostOwnTick=hostGuestTick=0;haveSample=false;nextSample=0;
        }
        void ClearNetwork() noexcept
        {
            if(protocol)protocol->Close();protocol.reset();ClearSceneTraffic();pendingAssignment.reset();
            guestSceneMismatch=guestSceneSuspended=false;
        }
        void DropPeer(const char* message)
        {
            failure=message;++metrics.protocolFailures;ClearNetwork();
            if(transport)transport->ClosePeer();
            if(!profile.paths.menuFirst && gameRole==SessionRole::Guest && Active())stopGameAfterRemoval=true;
            status={TextId::SessionClosedError,Wide(failure)};
        }
        void Detach() noexcept
        {
            ClearNetwork();transport=nullptr;
            if(!profile.paths.menuFirst && gameRole==SessionRole::Guest && Active())stopGameAfterRemoval=true;
            try{status={RetiredScene()?TextId::SceneEndedReconnect:
                Active() && profile.paths.menuFirst?TextId::DisconnectedGameOpen:
                Active() && gameRole==SessionRole::Host?TextId::GuestLeftRemoving:
                !allCompletedCleanup?TextId::DisconnectedCleanupUnknown:TextId::Disconnected};}catch(...){}
        }
        void StartGame(const SessionRole owner,const std::uint64_t now)
        {
            Require(!shutdown && !Active(),"An owned game is already active or shutting down");
            failure.clear();
            if(game){Require(game->Finished(),"Previous owned game has not completed cleanup");game.reset();}
            const auto id=FreshId();const auto root=GetFileAttributesW(profile.paths.caseRoot.c_str());
            if(root==INVALID_FILE_ATTRIBUTES)Require(CreateDirectoryW(profile.paths.caseRoot.c_str(),nullptr)!=0,"Cannot create the caseRoot directory");
            else Require((root&FILE_ATTRIBUTE_DIRECTORY) && !(root&FILE_ATTRIBUTE_REPARSE_POINT),"caseRoot is not an ordinary directory");
            GameProcessClientConfiguration config;config.game=profile.game;config.media=profile.paths.media;config.adapter=profile.paths.adapter;
            config.gameHost=profile.paths.helper;config.outer=profile.paths.outer;
            config.caseDirectory=profile.paths.caseRoot+L"\\P-"+std::to_wstring(GetCurrentProcessId())+L'-'+std::to_wstring(id);
            // Private generation intentionally has no relation to a connection's
            // protocol session or epoch. Host native game can span reconnects.
            Require(profile.paths.menuFirst || (profile.paths.seconds>=5 && profile.paths.seconds<=3600),"Legacy diagnostic duration must be5..3600 seconds");
            config.menuFirst=profile.paths.menuFirst;config.sceneFollow=config.menuFirst;
            if(config.menuFirst && owner==SessionRole::Guest)
            {
                Require(protocol && protocol->LevelEpoch() && protocol->AssignedLevel().levelId>=1 &&
                    protocol->AssignedLevel().levelId<=50 && protocol->AssignedLevel().entryId==0,
                    "Guest launch requires the server's actual ready scene and default entry");
                config.startupLevel=protocol->AssignedLevel().levelId;
                config.startupEntry=protocol->AssignedLevel().entryId;
            }
            config.session=id;config.levelEpoch=1;config.seconds=config.menuFirst?0:std::max(MinimumHelperLimitSeconds,profile.paths.seconds);
            metrics.lastGameHelperLimitSeconds=config.seconds;
            metrics.lastGameSoftLimitSeconds=config.menuFirst?0:std::min(profile.paths.seconds,config.seconds-GameLimitGraceSeconds);
            game=std::make_unique<GameProcessClient>(std::move(config));gameRole=owner;
            activeRemote.reset();desiredRemote.reset();nativePending.reset();haveScale=haveSample=false;readyCounted=false;
            stopRequested=stopGameAfterRemoval=gameSoftLimitReached=false;sceneRetired=guestSceneMismatch=waitingForVisualRemoval=false;
            guestSceneSuspended=false;pendingAssignment.reset();lastSceneGeneration=lastSceneRetirements=expectedFollowFromGeneration=0;
            stopBegan=0;lastSnapshotWorld=0;previousWorld=previousTime=0;nextSample=now;
            lastNativeCreated=lastNativeRemoved=lastDisplaySamples=0;
            // Earlier than the actual outer/helper/game CreateProcess, so a
            // slow Start cannot consume the native hard-bound cleanup grace.
            gameSoftStopAt=profile.paths.menuFirst?0:GetTickCount64()+std::uint64_t(metrics.lastGameSoftLimitSeconds)*1000;
            lastCooperativePump=0;
            ++metrics.gameStarts;
            Require(game->Start(),game->Failure().empty()?"Owned game start failed":game->Failure().c_str());
            status=profile.paths.menuFirst && owner==SessionRole::Guest?
                UiStatus{TextId::StartingClient,std::to_wstring(protocol->AssignedLevel().levelId)}:
                UiStatus{TextId::StartingHost};
        }
        bool CanAttach(const SessionRole candidate) const noexcept
        {
            if(shutdown || transport || !allCompletedCleanup)return false;
            if(!Active())return true;
            if(candidate!=gameRole || stopRequested || stopGameAfterRemoval || !game->Failure().empty())return false;
            if(RetiredScene())return profile.paths.menuFirst && candidate==SessionRole::Host;
            return candidate==SessionRole::Host || ReusableGuest();
        }
        bool Attach(WindowsTcpTransport& value,const SessionRole candidate)
        {
            if(!CanAttach(candidate))return false;ClearNetwork();transport=&value;role=candidate;failure.clear();
            status={role==SessionRole::Host?TextId::HostLobbyOpen:TextId::WaitingServerAssignment};return true;
        }
        bool StartHost(const std::uint64_t now)
        {
            if(shutdown || Active() || !allCompletedCleanup || (transport && role==SessionRole::Guest) || (!profile.paths.menuFirst && (!transport || role!=SessionRole::Host)))return false;
            StartGame(transport?role:SessionRole::Host,now);return true;
        }
        void CompleteGame(const bool finalEvidenceQualified=true,const char* finalValidationError=nullptr)
        {
            gameSoftStopAt=0;
            gameSoftLimitReached=false;
            lastResult=game->Result();hasResult=true;++metrics.gameCompletions;
            const bool facadeQualified=lastResult.successful && game->Failure().empty();
            const bool qualified=finalEvidenceQualified && facadeQualified;
            const bool nativeClean=qualified && !nativePending && (!activeRemote || (lastNativeCreated>0 && lastNativeCreated==lastNativeRemoved));
            if(nativeClean)++metrics.gameNativeCleanupCompletions;
            if(!lastResult.outerStarted && !lastResult.helperStarted)++metrics.gameNoOuterLaunchCompletions;
            // The existing owned helper may terminate its own child after a
            // requested stop. Preserve that fact in Result; process cleanup
            // additionally requires actual native Remove ACK/unload evidence.
            // A final, independently qualified retirement cancellation is kept
            // separate from Applied acknowledgements in both facade and metrics.
            const bool cleaned=Cleanup(lastResult) && nativeClean;
            allCompletedCleanup=allCompletedCleanup && cleaned;
            // The raw helper observations remain in lastResult. Coordinator
            // validation may only narrow its derived success, never erase a
            // first error or permit a new Start to discard unqualified cleanup.
            lastResult.successful=lastResult.successful && cleaned;
            if(lastResult.successful)++metrics.gameSuccessfulCompletions;
            if(!cleaned)
            {
                if(failure.empty())
                {
                    if(!game->Failure().empty())failure=game->Failure();
                    else if(finalValidationError && *finalValidationError)failure=finalValidationError;
                    else if(!lastResult.error.empty())failure=lastResult.error;
                    else failure="Final native lifecycle cleanup was not qualified";
                }
                if(lastResult.error.empty())lastResult.error=failure;
            }
            ClearNetwork();if(transport)transport->ClosePeer();
            activeRemote.reset();nativePending.reset();desiredRemote.reset();haveScale=haveSample=false;
            status=!cleaned?UiStatus{TextId::GameCleanupUnknownDetail,Wide(failure)}:
                UiStatus{lastResult.directHost?TextId::GameExited:
                !lastResult.outerStarted?TextId::GameNotStarted:TextId::GameExitedMediaRestored};
            // Retain the complete result rather than treating an empty facade as
            // successful cleanup. Only a subsequent explicit start releases it.
            stopRequested=true;
        }
        bool ReadNativeState(const std::uint64_t now)
        {
            bool observedRetirement=false;
            if(game->HasRuntimeSnapshot())
            {
                const auto& snapshot=game->Snapshot();
                if(snapshot.worldUpdates>lastSnapshotWorld){metrics.nativeFrames+=snapshot.worldUpdates-lastSnapshotWorld;lastSnapshotWorld=snapshot.worldUpdates;}
                Require(snapshot.remoteCreated>=lastNativeCreated && snapshot.remoteRemoved>=lastNativeRemoved,"Native lifecycle counter regressed");
                if(snapshot.remoteCreated!=lastNativeCreated)lastDisplaySamples=0;
                metrics.nativeRemoteCreated+=snapshot.remoteCreated-lastNativeCreated;metrics.nativeRemoteRemoved+=snapshot.remoteRemoved-lastNativeRemoved;
                lastNativeCreated=snapshot.remoteCreated;lastNativeRemoved=snapshot.remoteRemoved;
                Require(snapshot.remoteDisplaySamples>=lastDisplaySamples,"Native display counter regressed within an actor cohort");
                metrics.remoteDisplaySamples+=snapshot.remoteDisplaySamples-lastDisplaySamples;lastDisplaySamples=snapshot.remoteDisplaySamples;
                metrics.remoteDisplayMaxGapMs=std::max(metrics.remoteDisplayMaxGapMs,static_cast<std::uint64_t>(snapshot.remoteDisplayMaxGapMs));
                metrics.remoteDisplayMaxStep=std::max(metrics.remoteDisplayMaxStep,snapshot.remoteDisplayMaxStep);
                Require(snapshot.sceneGeneration>=lastSceneGeneration && snapshot.sceneRetirements>=lastSceneRetirements,
                    "Native scene identity counter regressed");
                observedRetirement=snapshot.sceneRetirements>lastSceneRetirements;
                if(observedRetirement)
                {
                    Require(!snapshot.remoteEntity && snapshot.remoteCreated==snapshot.remoteRemoved,
                        "New native scene has no actual old-cohort cleanup");
                    metrics.nativeEpochRetirements+=snapshot.sceneRetirements-lastSceneRetirements;
                    activeRemote.reset();ClearSceneTraffic();haveScale=readyCounted=false;appliedRevision=0;
                }
                lastSceneGeneration=snapshot.sceneGeneration;lastSceneRetirements=snapshot.sceneRetirements;
            }
            // The helper can exit between GUI pumps. Consume a real final ACK
            // and lifecycle snapshot before deciding CompleteGame's cleanup.
            ReadNativeAck(now);
            if(observedRetirement && profile.paths.menuFirst && protocol)
            {
                if(role==SessionRole::Host && protocol->LevelEpoch() && !protocol->SceneSuspended())
                    Require(protocol->HostSuspendLevel(1),"Host could not fence its actually retired native scene");
                else if(role==SessionRole::Guest && (!expectedFollowFromGeneration || game->Snapshot().sceneRetirements>expectedFollowFromGeneration))
                {
                    // Original client menu/quit is still an explicit departure,
                    // not a request to load another scene automatically.
                    ClearNetwork();if(transport)transport->ClosePeer();
                }
            }
            const bool retired=profile.paths.menuFirst && game->HasRuntimeSnapshot() && game->Snapshot().epochRetired;
            if(retired)
            {
                const auto& snapshot=game->Snapshot();
                Require(snapshot.phase==adapter::RuntimePhase::Stopped && snapshot.error==adapter::RuntimeError::None && !snapshot.actorError &&
                    !snapshot.remoteEntity && snapshot.remoteCreated==snapshot.remoteRemoved,"Retired visual epoch lacks terminal passive cleanup");
                if(game->Finished() && game->Result().commandCancelledOnRetirement)
                {
                    // This final facade result pins the outstanding immutable
                    // command to a retired, cleaned native generation. It was
                    // not Applied: preserve the raw unconfirmed flag and do not
                    // increase nativeAcknowledgements or any operation ACK count.
                    const auto& result=game->Result();const auto& ack=game->Acknowledgement();
                    Require(nativePending && result.commandUnconfirmed && result.commandsPublished==nativePending->sequence &&
                        result.commandsCompleted<nativePending->sequence && nativePending->sequence-result.commandsCompleted==1 &&
                        ack.sequence==result.commandsCompleted && snapshot.lastCommandSequence==result.commandsCompleted,
                        "Retirement cancellation differs from the outstanding native command");
                    ++metrics.nativeCommandsCancelledOnRetirement;nativePending.reset();
                }
                sceneRetired=true;activeRemote.reset();desiredRemote.reset();haveScale=readyCounted=false;appliedRevision=0;
                if(!game->Finished() && profile.paths.menuFirst)
                    status={TextId::SceneUnloaded};
                else
                {ClearNetwork();if(transport && transport->ActiveConnectionId())transport->ClosePeer();}
            }
            return retired;
        }
        void ReadGame(const std::uint64_t now)
        {
            if(!game)return;const bool wasFinished=game->Finished();game->Pump();
            if(wasFinished && metrics.gameCompletions>=metrics.gameStarts)return;
            if(game->Finished())
            {
                // FinishedFailed is terminal too. A bad final ACK/retirement
                // must produce one retained failed completion, not throw on
                // every pump before lastResult/cleanup accounting is written.
                try{ReadNativeState(now);}
                catch(const std::exception& error){CompleteGame(false,error.what());return;}
                catch(...){CompleteGame(false,"Unknown final native lifecycle validation error");return;}
                CompleteGame();return;
            }
            const bool retired=ReadNativeState(now);
            if(!game->Failure().empty())
            {if(failure.empty())failure=game->Failure();desiredRemote.reset();if(protocol){ClearNetwork();if(transport)transport->ClosePeer();}status={TextId::AdapterError,Wide(failure)};}
            if(retired)return;
            if(game->Ready() && !readyCounted)
            {
                Require(game->Snapshot().levelId>=1 && game->Snapshot().levelId<=50 && !game->Snapshot().epochRetired,"Original Playing observation is not ready");
                if(profile.paths.menuFirst) profile.level.levelId=game->Snapshot().levelId;
                if(expectedFollowFromGeneration && game->Snapshot().sceneGeneration>expectedFollowFromGeneration)
                    expectedFollowFromGeneration=0;
                readyCounted=true;sceneRetired=false;++metrics.gameReady;sceneTransform=game->Snapshot().local;
                for(const auto scale:sceneTransform.scale)Require(std::isfinite(scale) && scale>0,"Qualified local scale is invalid");
                haveScale=true;status={TextId::LocationLoaded,std::to_wstring(profile.level.levelId)};
            }
        }
        void ReadNativeAck(const std::uint64_t now)
        {
            if(!nativePending || !game)return;const auto& ack=game->Acknowledgement();
            if(ack.sequence<nativePending->sequence)return;
            Require(ack.sequence==nativePending->sequence && ack.entity==nativePending->key.entity && ack.operation==nativePending->operation &&
                ack.session==game->Identity().session && ack.levelEpoch==game->Identity().levelEpoch && ack.sceneGeneration==nativePending->sceneGeneration &&
                ack.error==adapter::RuntimeError::None && !ack.actorError &&
                (ack.status==adapter::RuntimeAckStatus::Applied || ack.status==adapter::RuntimeAckStatus::CancelledOnRetirement),
                "Native visual acknowledgement does not match the immutable outstanding command");
            const bool assignmentMatches=pendingAssignment && pendingAssignment->key==nativePending->key;
            if(ack.status==adapter::RuntimeAckStatus::CancelledOnRetirement)
            {
                Require(game->HasRuntimeSnapshot() && game->Snapshot().sceneRetirements>=ack.sceneGeneration,
                    "Native cancellation lacks its actual scene-retirement receipt");
                ++metrics.nativeCommandsCancelledOnRetirement;
                if(ack.operation==adapter::RuntimeOperation::FollowLevel && expectedFollowFromGeneration==ack.sceneGeneration)
                    expectedFollowFromGeneration=0;
                if(ack.operation==adapter::RuntimeOperation::FollowLevel && assignmentMatches)
                    pendingAssignment->requested=pendingAssignment->requestAcknowledged=false;
            }
            else
            {
                Require(now>=nativePending->began,"Native command clock regressed");const auto elapsed=now-nativePending->began;
                ++metrics.nativeAcknowledgements;metrics.nativeAckLatencyTotalMs+=elapsed;metrics.nativeAckLatencyMaxMs=std::max(metrics.nativeAckLatencyMaxMs,elapsed);
                const bool currentScene=game->HasRuntimeSnapshot() && !game->Snapshot().epochRetired &&
                    game->Snapshot().sceneGeneration==ack.sceneGeneration && game->Snapshot().sceneRetirements<ack.sceneGeneration;
                if(ack.operation==adapter::RuntimeOperation::Create)
                {if(currentScene){activeRemote=nativePending->key;appliedRevision=nativePending->revision;}++metrics.nativeCreatesAcknowledged;}
                else if(ack.operation==adapter::RuntimeOperation::Apply)
                {if(currentScene)appliedRevision=nativePending->revision;++metrics.nativeAppliesAcknowledged;}
                else if(ack.operation==adapter::RuntimeOperation::Remove)
                {if(activeRemote && *activeRemote==nativePending->key){activeRemote.reset();appliedRevision=0;}++metrics.nativeRemovesAcknowledged;}
                else if(ack.operation==adapter::RuntimeOperation::FollowLevel)
                {++metrics.nativeFollowRequestsAcknowledged;if(assignmentMatches)pendingAssignment->requestAcknowledged=true;}
            }
            nativePending.reset();
        }
        bool Issue(const adapter::RuntimeOperation operation,const RemoteKey& key,const Visual* visual,const std::uint64_t now)
        {
            adapter::Transform transform=sceneTransform;adapter::RuntimeAnimation animation=adapter::RuntimeAnimation::Idle;
            if(visual)
            {
                Require(NumericEnvelope(visual->state),"Visual target exceeds native numeric limits");
                std::copy(visual->state.position.begin(),visual->state.position.end(),transform.position);
                std::copy(visual->state.rotation.begin(),visual->state.rotation.end(),transform.orientation);
                animation=static_cast<adapter::RuntimeAnimation>(visual->state.animation);
            }
            if(!game->Submit(operation,key.entity,transform,animation))
            {Require(game->Failure().empty(),game->Failure().c_str());return false;}
            nativePending=NativePending{key,operation,game->Result().commandsPublished,now,visual?visual->revision:0,game->Snapshot().sceneGeneration};++metrics.nativeCommands;return true;
        }
        void DriveVisual(const std::uint64_t now)
        {
            if(!Active())return;
            const bool remove=activeRemote && (!desiredRemote || !(desiredRemote->key==*activeRemote));
            // Removal has its own exact retained-scene admission. A paused
            // snapshot deliberately has no levelId/scale, so do not require
            // Playing readiness for Remove. Create/Apply remain Ready-only.
            if(remove && !nativePending && !game->CommandPending() && game->CanSubmitRemoval())
                Issue(adapter::RuntimeOperation::Remove,*activeRemote,nullptr,now);
            else if(!remove && game->Ready() && haveScale && game->Snapshot().levelId==profile.level.levelId && !nativePending && !game->CommandPending())
            {
                if(desiredRemote && !activeRemote)Issue(adapter::RuntimeOperation::Create,desiredRemote->key,&*desiredRemote,now);
                else if(desiredRemote && activeRemote && desiredRemote->revision>appliedRevision)Issue(adapter::RuntimeOperation::Apply,desiredRemote->key,&*desiredRemote,now);
            }
            if(remove && (!nativePending || nativePending->operation!=adapter::RuntimeOperation::Remove))
            {if(!waitingForVisualRemoval){waitingForVisualRemoval=true;++metrics.visualRemovalWaits;}}
            else waitingForVisualRemoval=false;
            if((shutdown || stopGameAfterRemoval) && !stopRequested)
            {
                if(!stopBegan)stopBegan=now;
                // Prefer an actual Remove ACK. If the game has retired the
                // scene or stops responding, its owned lifecycle is responsible
                // for unload cleanup and reports any unconfirmed command.
                const bool retired=game->HasRuntimeSnapshot() && game->Snapshot().epochRetired;
                const bool removalTimedOut=now>=stopBegan && now-stopBegan>=RemoveWaitMilliseconds;
                if((!activeRemote && !nativePending) || retired || !game->Failure().empty() || removalTimedOut)
                {
                    if(gameSoftLimitReached && removalTimedOut && (activeRemote || nativePending))++metrics.gameSoftLimitFallbackStops;
                    game->RequestStop();stopRequested=true;status={profile.paths.menuFirst?TextId::StoppingGame:TextId::StoppingGameMedia};
                }
            }
        }
        void DriveSceneFollow(const std::uint64_t now)
        {
            if(shutdown || stopRequested || stopGameAfterRemoval || !profile.paths.menuFirst || role!=SessionRole::Guest ||
                !protocol || protocol->CurrentPhase()!=gameplay::Phase::Loading || !pendingAssignment ||
                !pendingAssignment->needsFollow || pendingAssignment->requested || !Active() || !game->Ready() ||
                activeRemote || nativePending || game->CommandPending())return;
            Require(pendingAssignment->key.connection==protocol->ConnectionId() && pendingAssignment->key.session==protocol->SessionId() &&
                pendingAssignment->key.epoch==protocol->LevelEpoch() && pendingAssignment->level==protocol->AssignedLevel(),
                "Guest follow assignment differs from the current protocol epoch");
            const auto generation=game->Snapshot().sceneGeneration;
            if(!game->RequestSceneFollow(pendingAssignment->level.levelId,pendingAssignment->level.entryId))
            {Require(game->Failure().empty(),game->Failure().c_str());return;}
            pendingAssignment->fromNativeGeneration=generation;pendingAssignment->requested=true;
            expectedFollowFromGeneration=generation;
            nativePending=NativePending{pendingAssignment->key,adapter::RuntimeOperation::FollowLevel,
                game->Result().commandsPublished,now,0,generation};++metrics.nativeCommands;
            status={TextId::FollowingLocation,std::to_wstring(pendingAssignment->level.levelId)};
        }
        void CheckGameLimit(const std::uint64_t now)
        {
            if(!Active() || !gameSoftStopAt || now<gameSoftStopAt || shutdown || stopGameAfterRemoval || stopRequested || !game->Failure().empty())return;
            ++metrics.gameSoftLimitStops;
            gameSoftLimitReached=true;
            // Do not let already queued observations or a newly accepted TCP
            // peer restart gameplay while the native generation is retiring.
            if(transport)transport->ClosePeer();
            ClearNetwork();stopGameAfterRemoval=true;stopBegan=now;lastCooperativePump=0;
            status={TextId::PrototypeTimeLimit};
        }
        void Flush(const std::uint64_t now)
        {
            if(!protocol || !transport)return;
            for(std::size_t count=0;count<gameplay::GameplayProtocol::MaximumOutbound;++count)
            {
                const auto* message=protocol->PeekOutbound();if(!message)break;
                if(!transport->SendApplicationMessage(protocol->ConnectionId(),message->classId,message->key,message->payload.data(),static_cast<std::uint16_t>(message->payload.size()),now))break;
                Require(protocol->CommitOutbound(),"Gameplay outbox commit failed");++metrics.messagesSent;
            }
        }
        void StartProtocol()
        {
            const auto connection=transport->ActiveConnectionId();gameplay::Config config;config.role=role;
            config.compatibility=profile.compatibility;config.qualifiedLevel=profile.level;
            config.allowDeferredLevel=profile.paths.menuFirst;config.allowSceneFollow=config.allowDeferredLevel;
            if(config.allowDeferredLevel)config.qualifiedLevel.levelId=0;
            config.hostSession=role==SessionRole::Host?FreshId():0;
            protocol=std::make_unique<gameplay::GameplayProtocol>(config);Require(protocol->Begin(connection),"Cannot begin gameplay protocol for this carrier");
            sourceTick=hostOwnTick=hostGuestTick=0;haveSample=false;receipts.clear();decisions.clear();
        }
        void QueueVisual(const gameplay::Event& event)
        {
            Require(protocol && event.entity==protocol->RemoteEntity() && event.state.entity==event.entity,"Only the accepted remote entity may reach native display");
            Require(NumericEnvelope(event.state),"Accepted remote pose exceeds the bounded native scene envelope");
            const auto publishedRevision=nativePending?std::max(appliedRevision,nativePending->revision):appliedRevision;
            if(desiredRemote && desiredRemote->revision>publishedRevision)++metrics.visualStatesCoalesced;
            desiredRemote=Visual{{protocol->ConnectionId(),event.session,event.epoch,event.entity},event.state,++visualRevision};++metrics.remoteStates;
        }
        void Receipt(const gameplay::Event& event,const std::uint64_t now)
        {
            Require(!receipts.empty() && receipts.front().sequence==event.observationToken.sequence && now>=receipts.front().began,
                "Host decision does not match the local observation receipt FIFO");
            const auto elapsed=now-receipts.front().began;receipts.pop_front();++metrics.acknowledgementsReceived;
            ++metrics.observationRttSamples;metrics.observationRttTotalMs+=elapsed;metrics.observationRttMaxMs=std::max(metrics.observationRttMaxMs,elapsed);
            if(event.kind==gameplay::EventKind::ObservationRejected)++metrics.observationsRejected;
        }
        void Events(const std::uint64_t now)
        {
            if(!protocol)return;
            for(std::size_t count=0;count<gameplay::GameplayProtocol::MaximumEvents*2;++count)
            {
                const auto event=protocol->TakeEvent();if(!event)break;
                Require(Current(*event),"Gameplay event has a stale generation");
                switch(event->kind)
                {
                case gameplay::EventKind::LevelAssigned:
                    // A fast host may queue another control epoch before these
                    // events are drained. Only the latest assignment can drive
                    // a native request; older controls remain ordered evidence.
                    if(event->epoch<protocol->LevelEpoch())break;
                    Require(!profile.paths.menuFirst || (event->level.levelId>=1 && event->level.levelId<=50),"Server assigned an invalid Playing scene");
                    if(!profile.paths.menuFirst)Require(event->level==profile.level,"Server assigned an unqualified scene");
                    if(role==SessionRole::Guest)
                    {
                        const RemoteKey assignmentKey{protocol->ConnectionId(),event->session,event->epoch,0};
                        if(!Active())
                        {
                            StartGame(SessionRole::Guest,now);
                            pendingAssignment=SceneAssignment{assignmentKey,event->level,0,false,false,false};
                        }
                        else
                        {
                            const bool reuse=ReusableGuest() && !guestSceneSuspended && profile.level==event->level;
                            if(reuse)
                            {
                                pendingAssignment=SceneAssignment{assignmentKey,event->level,0,false,false,false};
                                ++metrics.guestSceneReuses;
                                status={TextId::SameLocationReconnect};
                            }
                            else if(profile.paths.menuFirst && gameRole==SessionRole::Guest && !stopRequested && !stopGameAfterRemoval && game->Failure().empty())
                            {
                                pendingAssignment=SceneAssignment{assignmentKey,event->level,0,true,false,false};
                                guestSceneMismatch=false;
                                status={TextId::NewSceneAssigned};
                            }
                            else
                            {
                                guestSceneMismatch=true;++metrics.sceneMismatchWaits;
                                status={TextId::GuestSceneUnavailable};
                            }
                        }
                    }
                    break;
                case gameplay::EventKind::SceneSuspended:
                    ++metrics.sceneSuspensions;ClearSceneTraffic();pendingAssignment.reset();
                    if(role==SessionRole::Guest)guestSceneSuspended=true;
                    status={TextId::ServerUnloadingScene};
                    break;
                case gameplay::EventKind::Started:++metrics.cooperativeStarts;status={TextId::PlayersTogether};break;
                case gameplay::EventKind::ObservedState:
                    Require(role==SessionRole::Host && decisions.size()<gameplay::GameplayProtocol::MaximumPendingObservations,"Guest observation decision FIFO overflow");
                    decisions.push_back({event->observationToken,NumericEnvelope(event->observed)});++metrics.observationsReceived;break;
                case gameplay::EventKind::AcceptedState:QueueVisual(*event);break;
                case gameplay::EventKind::ObservationAccepted:Receipt(*event,now);break; // ACK only; never touch guest local root.
                case gameplay::EventKind::ObservationRejected:
                    if(role==SessionRole::Guest)Receipt(*event,now);break;
                case gameplay::EventKind::EntityRemoved:
                    if(event->entity==protocol->RemoteEntity())desiredRemote.reset();break;
                case gameplay::EventKind::Disconnected:desiredRemote.reset();break;
                default:throw std::runtime_error("Unsupported gameplay event reached the coordinator");
                }
            }
        }
        void Decide(const std::uint64_t now)
        {
            if(shutdown || stopGameAfterRemoval || stopRequested || !protocol || role!=SessionRole::Host || protocol->CurrentPhase()!=gameplay::Phase::Running)return;
            while(!decisions.empty() && protocol->OutboundCount()<gameplay::GameplayProtocol::MaximumOutbound-2 && protocol->EventCount()<gameplay::GameplayProtocol::MaximumEvents-2)
            {
                // V4 policy accepts an exactly identified, structurally checked
                // observation of stock local movement. This is not native host
                // physics, collision validation or an anti-cheat speed test.
                Require(game && Active() && gameRole==SessionRole::Host && !stopRequested && !stopGameAfterRemoval,
                    "Host observation decision has no active owned game");
                const bool hasSnapshot=game->HasRuntimeSnapshot();
                if(hasSnapshot)
                {
                    const auto& snapshot=game->Snapshot();const auto& identity=game->Identity();
                    Require(snapshot.gamePid==identity.gamePid && snapshot.mainThreadId==identity.mainThreadId &&
                        snapshot.mode==adapter::RuntimeMode::PassiveActor && snapshot.session==identity.session &&
                        snapshot.levelEpoch==identity.levelEpoch && snapshot.sceneGeneration && !snapshot.epochRetired,
                        "Host observation decision crossed its owned native scene identity");
                }
                if(!CurrentHostObservation(hasSnapshot,game->Ready(),game->Snapshot().monotonicMilliseconds,now))
                {
                    // Pauses, cinematics and other transient states may stop
                    // World publication without retiring the owned scene or
                    // reporting an adapter error. Consume the FIFO as Busy;
                    // absence/staleness is never a request to stop the game.
                    Require(protocol->RejectObservedState(decisions.front().token,2),"Host busy-scene rejection failed");++metrics.observationsRejected;
                }
                else
                {
                    // A retained paused scene deliberately exports level0.
                    // Only actual Ready can qualify a level for acceptance.
                    Require(game->Snapshot().levelId==profile.level.levelId,
                        "Host ready observation differs from its assigned level");
                    if(decisions.front().withinNumericEnvelope)
                    {Require(protocol->AcceptObservedState(decisions.front().token,Next(hostGuestTick)),"Host observation decision failed");++metrics.observationsAccepted;}
                    else
                    {Require(protocol->RejectObservedState(decisions.front().token,1),"Host bounded-pose rejection failed");++metrics.observationsRejected;}
                }
                decisions.pop_front();
                Events(now);Flush(now);
            }
        }
        void Network(const std::uint64_t now)
        {
            if(!transport || shutdown)return;
            if(RetiredScene() && !profile.paths.menuFirst)
            {
                // The old native epoch is terminal. Keep the original game
                // running, but never welcome a new peer into that scene.
                if(transport->ActiveConnectionId())transport->ClosePeer();
                return;
            }
            if(Active() && (stopGameAfterRemoval || stopRequested))
            {if(transport->ActiveConnectionId())transport->ClosePeer();return;}
            const auto connection=transport->ActiveConnectionId();const auto* carrier=transport->Session();
            const bool running=connection && carrier && carrier->Phase()==SessionPhase::Running;
            if(protocol && (!running || protocol->ConnectionId()!=connection))
            {
                ClearNetwork();if(!profile.paths.menuFirst && gameRole==SessionRole::Guest && Active())stopGameAfterRemoval=true;
                if(!Active())
                    status={role==SessionRole::Host?TextId::HostLobbyOpen:TextId::Disconnected};
                else
                    status={profile.paths.menuFirst?TextId::ConnectionClosedGameOpen:
                        role==SessionRole::Host?TextId::GuestLeftWaiting:TextId::ServerLeftClosingGuest};
            }
            if(running && !protocol)
            {
                if(role==SessionRole::Guest && (stopRequested || stopGameAfterRemoval) && Active())return;
                StartProtocol();
            }
            if(!protocol)return;
            Flush(now);
            for(std::size_t count=0;count<WindowsTcpTransport::MaximumDeliveredMessages;++count)
            {
                const auto message=transport->TakeMessage();if(!message)break;
                if(message->connectionId!=protocol->ConnectionId()){++metrics.wrongConnections;continue;}
                const auto result=protocol->Receive(message->connectionId,message->message);++metrics.messagesReceived;
                if(result==gameplay::ReceiveResult::WrongConnection){++metrics.wrongConnections;continue;}
                if(result==gameplay::ReceiveResult::IgnoredStale)++metrics.staleMessages;
                if(result==gameplay::ReceiveResult::Rejected){DropPeer("Gameplay protocol rejected an incompatible, malformed or unauthorized message");return;}
                Events(now);Decide(now);Flush(now);
            }
            if(role==SessionRole::Host && game && game->Ready() && !stopRequested && !activeRemote && !nativePending && !game->CommandPending() &&
                (!protocol->LevelEpoch() || (protocol->SceneSuspended() && game->Snapshot().sceneGeneration>protocol->LevelEpoch())))
            {
                Require(protocol->HostAssignLevel(profile.paths.menuFirst?game->Snapshot().sceneGeneration:1,profile.level),
                    "Host failed to assign its actually observed Playing scene generation");
                ++metrics.hostSceneAssignments;
            }
            if(role==SessionRole::Guest && game && game->Ready() && !guestSceneMismatch && !activeRemote && !nativePending && !game->CommandPending() &&
                pendingAssignment && pendingAssignment->key.epoch==protocol->LevelEpoch() &&
                protocol->LevelEpoch() && !protocol->LocalReady() && profile.level==protocol->AssignedLevel() && game->Snapshot().levelId==protocol->AssignedLevel().levelId &&
                (!pendingAssignment->needsFollow || (pendingAssignment->requested && pendingAssignment->requestAcknowledged &&
                    game->Snapshot().sceneGeneration>pendingAssignment->fromNativeGeneration)))
            {
                Require(protocol->NotifyLocalReady(profile.level),"Guest failed its actual new-scene ready barrier");
                if(pendingAssignment->needsFollow)++metrics.guestSceneFollows;
                pendingAssignment.reset();guestSceneSuspended=false;
            }
            Events(now);Decide(now);Flush(now);
            if(protocol && (protocol->CurrentPhase()==gameplay::Phase::Failed || protocol->CurrentPhase()==gameplay::Phase::Closed))DropPeer("Gameplay protocol terminated");
        }
        void Sample(const std::uint64_t now)
        {
            if(shutdown || stopGameAfterRemoval || !protocol || protocol->CurrentPhase()!=gameplay::Phase::Running || !game || !game->Ready() || stopRequested || now<nextSample)return;
            nextSample=now+50;
            if(protocol->OutboundCount()>=gameplay::GameplayProtocol::MaximumOutbound-2 ||
                (role==SessionRole::Guest && protocol->PendingObservationCount()>=gameplay::GameplayProtocol::MaximumPendingObservations-2))
            {++metrics.deferredSamples;return;}
            const auto& snapshot=game->Snapshot();
            if(snapshot.monotonicMilliseconds<=previousTime || snapshot.worldUpdates<=previousWorld){++metrics.deferredSamples;return;}
            gameplay::ObservedState observed;observed.entity=protocol->LocalEntity();
            if(!Basis(snapshot.local.orientation,observed.rotation)){++metrics.invalidSamples;return;}
            for(unsigned i=0;i<3;++i)
            {
                if(!std::isfinite(snapshot.local.position[i])){++metrics.invalidSamples;return;}
                observed.position[i]=snapshot.local.position[i];
            }
            // Time is local native monotonic time, never a peer timestamp. A
            // first/restarted or >1s sampling gap has unknown velocity (zero).
            if(haveSample && snapshot.monotonicMilliseconds>previousTime && snapshot.monotonicMilliseconds-previousTime<=1000)
            {
                const auto seconds=static_cast<double>(snapshot.monotonicMilliseconds-previousTime)/1000.0;
                for(unsigned i=0;i<3;++i)
                {
                    const auto speed=(static_cast<double>(snapshot.local.position[i])-previousLocal.position[i])/seconds;
                    if(!std::isfinite(speed) || std::abs(speed)>1000000.0){++metrics.invalidSamples;return;}
                    observed.velocity[i]=static_cast<float>(speed);
                }
            }
            if(!NumericEnvelope(observed)){++metrics.invalidSamples;return;}
            if(snapshot.localAnimationQualified && snapshot.localAnimation<=adapter::RuntimeAnimation::Jump)
            {observed.animation=static_cast<gameplay::Animation>(snapshot.localAnimation);++metrics.nativeAnimationSamples;}
            else
            {
                // Fallback is an explicitly approximate visual classifier in
                // native units/s: Y vertical >0.35, horizontal >0.05. It never
                // writes stock input/FSM or claims to recover gameplay state.
                const auto horizontal=std::hypot(static_cast<double>(observed.velocity[0]),static_cast<double>(observed.velocity[2]));
                observed.animation=std::abs(observed.velocity[1])>0.35f?gameplay::Animation::Jump:horizontal>0.05?gameplay::Animation::Walk:gameplay::Animation::Idle;
                ++metrics.derivedAnimationSamples;
            }
            observed.observationTick=Next(sourceTick);
            if(role==SessionRole::Guest)
            {
                Require(protocol->SubmitObservedState(observed),"Guest observation submit failed");
                receipts.push_back({protocol->LastGuestObservationSequence(),now});++metrics.observationsSent;
            }
            else
            {
                gameplay::AcceptedState accepted;accepted.entity=gameplay::GameplayProtocol::HostEntity;accepted.hostTick=Next(hostOwnTick);
                accepted.position=observed.position;accepted.velocity=observed.velocity;accepted.rotation=observed.rotation;accepted.animation=observed.animation;
                Require(protocol->PublishAcceptedState(accepted),"Host local state publication failed");
            }
            previousLocal=snapshot.local;previousTime=snapshot.monotonicMilliseconds;previousWorld=snapshot.worldUpdates;haveSample=true;++metrics.sampledStates;
        }
        void Pump(const std::uint64_t now)
        {
            ReadGame(now);CheckGameLimit(now);Network(now);Sample(now);Flush(now);DriveVisual(now);DriveSceneFollow(now);
            const bool together=protocol && protocol->CurrentPhase()==gameplay::Phase::Running && game && game->Ready() &&
                activeRemote && !stopRequested && !stopGameAfterRemoval && now>=game->Snapshot().monotonicMilliseconds &&
                now-game->Snapshot().monotonicMilliseconds<=1000;
            if(together && lastCooperativePump && now>=lastCooperativePump)metrics.cooperativeRunningMs+=now-lastCooperativePump;
            lastCooperativePump=together?now:0;
        }
        void Shutdown() noexcept
        {
            shutdown=true;ClearNetwork();desiredRemote.reset();stopGameAfterRemoval=true;
            if(!Active()){try{status={TextId::ClosingLobby};}catch(...){}}
        }
        void Fail(const char* message) noexcept
        {
            try{failure=message;status={TextId::CoopError,Wide(failure)};}catch(...){}
            if(transport)transport->ClosePeer();ClearNetwork();stopGameAfterRemoval=true;
            if(game && Active()){game->RequestStop();stopRequested=true;}
        }
    };
    ReplicationCoordinator::ReplicationCoordinator(PrototypeProfile profile):implementation_(std::make_unique<Impl>(std::move(profile))){}
    ReplicationCoordinator::~ReplicationCoordinator()=default;
    bool ReplicationCoordinator::Attach(WindowsTcpTransport& transport,const SessionRole role){return implementation_->Attach(transport,role);}
    void ReplicationCoordinator::Detach() noexcept{implementation_->Detach();}
    bool ReplicationCoordinator::StartHostGame(const std::uint64_t now)
    {try{return implementation_->StartHost(now);}catch(const std::exception& error){implementation_->Fail(error.what());}catch(...){implementation_->Fail("Unknown game start error");}return false;}
    void ReplicationCoordinator::Pump(const std::uint64_t now) noexcept
    {try{implementation_->Pump(now);}catch(const std::exception& error){implementation_->Fail(error.what());}catch(...){implementation_->Fail("Unknown replication error");}}
    void ReplicationCoordinator::RequestShutdown() noexcept{implementation_->Shutdown();}
    bool ReplicationCoordinator::Finished() const noexcept{return implementation_->shutdown && !implementation_->Active();}
    bool ReplicationCoordinator::CanStartHostGame() const noexcept{return !implementation_->shutdown && implementation_->allCompletedCleanup && !implementation_->Active() && !(implementation_->transport && implementation_->role==SessionRole::Guest) && (implementation_->profile.paths.menuFirst || (implementation_->transport && implementation_->role==SessionRole::Host));}
    bool ReplicationCoordinator::GameActive() const noexcept{return implementation_->Active();}
    bool ReplicationCoordinator::CanAttach(const SessionRole role) const noexcept{return implementation_->CanAttach(role);}
    const UiStatus& ReplicationCoordinator::Status() const noexcept{return implementation_->status;}
    const std::string& ReplicationCoordinator::Failure() const noexcept{return implementation_->failure;}
    const ReplicationMetrics& ReplicationCoordinator::Metrics() const noexcept{return implementation_->metrics;}
    const GameProcessClientResult& ReplicationCoordinator::LastGameResult() const noexcept{return implementation_->lastResult;}
    bool ReplicationCoordinator::HasGameResult() const noexcept{return implementation_->hasResult;}
    bool ReplicationCoordinator::CleanupConfirmed() const noexcept
    {return !implementation_->Active() && implementation_->metrics.gameCompletions==implementation_->metrics.gameStarts && implementation_->allCompletedCleanup;}
}
