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
        {RemoteKey key;adapter::RuntimeOperation operation{};std::uint32_t sequence{};std::uint64_t began{},revision{};};
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
        std::wstring status=L"Профиль Alfea 28 проверен. Создайте лобби или подключитесь к серверу.";
        std::optional<RemoteKey> activeRemote;
        std::optional<Visual> desiredRemote;
        std::optional<NativePending> nativePending;
        std::deque<ObservationDecision> decisions;
        std::deque<ObservationReceipt> receipts;
        adapter::Transform sceneTransform{},previousLocal{};
        std::uint64_t nextSample{},previousTime{},previousWorld{},visualRevision{},appliedRevision{},stopBegan{},lastSnapshotWorld{};
        std::uint64_t lastCooperativePump{};
        std::uint64_t gameSoftStopAt{};
        std::uint32_t lastNativeCreated{},lastNativeRemoved{},lastDisplaySamples{};
        std::uint32_t sourceTick{},hostOwnTick{},hostGuestTick{};
        bool shutdown{},stopGameAfterRemoval{},stopRequested{},gameSoftLimitReached{},readyCounted{},haveSample{},haveScale{},hasResult{},allCompletedCleanup=true;

        explicit Impl(PrototypeProfile value):profile(std::move(value))
        {Require(profile.filePins && profile.level.levelId==28 && profile.level.entryId==0,"Only a byte-pinned qualified Stage P Alfea28 entry0 profile is supported");}
        bool Active() const noexcept{return game && !game->Finished();}
        bool Current(const gameplay::Event& event) const noexcept
        {return protocol && event.session==protocol->SessionId() && event.epoch==protocol->LevelEpoch();}
        void ClearNetwork() noexcept
        {
            if(protocol)protocol->Close();protocol.reset();decisions.clear();receipts.clear();desiredRemote.reset();
            sourceTick=hostOwnTick=hostGuestTick=0;haveSample=false;nextSample=0;
        }
        void DropPeer(const char* message)
        {
            failure=message;++metrics.protocolFailures;ClearNetwork();
            if(transport)transport->ClosePeer();
            if(gameRole==SessionRole::Guest && Active())stopGameAfterRemoval=true;
            status=L"Сетевой сеанс закрыт: "+Wide(failure);
        }
        void Detach() noexcept
        {
            ClearNetwork();transport=nullptr;
            if(gameRole==SessionRole::Guest && Active())stopGameAfterRemoval=true;
            try{status=Active() && gameRole==SessionRole::Host?L"Гость отключён. Игра сервера продолжает работать; удаляем его модель.":L"Сеанс отключён. Ожидаем завершения очистки игры.";}catch(...){}
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
            Require(profile.paths.seconds>=5 && profile.paths.seconds<=3600,"Prototype game duration must be5..3600 seconds");
            config.session=id;config.levelEpoch=1;config.seconds=std::max(MinimumHelperLimitSeconds,profile.paths.seconds);
            metrics.lastGameHelperLimitSeconds=config.seconds;
            metrics.lastGameSoftLimitSeconds=std::min(profile.paths.seconds,config.seconds-GameLimitGraceSeconds);
            game=std::make_unique<GameProcessClient>(std::move(config));gameRole=owner;
            activeRemote.reset();desiredRemote.reset();nativePending.reset();haveScale=haveSample=false;readyCounted=false;
            stopRequested=stopGameAfterRemoval=gameSoftLimitReached=false;stopBegan=0;lastSnapshotWorld=0;previousWorld=previousTime=0;nextSample=now;
            lastNativeCreated=lastNativeRemoved=lastDisplaySamples=0;
            // Earlier than the actual outer/helper/game CreateProcess, so a
            // slow Start cannot consume the native hard-bound cleanup grace.
            gameSoftStopAt=GetTickCount64()+std::uint64_t(metrics.lastGameSoftLimitSeconds)*1000;
            lastCooperativePump=0;
            ++metrics.gameStarts;
            Require(game->Start(),game->Failure().empty()?"Owned game start failed":game->Failure().c_str());
            status=owner==SessionRole::Host?L"Сервер запускает Alfea 28. Гость ждёт фактической готовности уровня.":L"Сервер назначил Alfea 28. Запускаем локальную игру и ждём готовности уровня.";
        }
        bool CanAttach(const SessionRole candidate) const noexcept
        {return !shutdown && !transport && allCompletedCleanup && (!Active() || (candidate==SessionRole::Host && gameRole==candidate && !stopRequested && !stopGameAfterRemoval));}
        bool Attach(WindowsTcpTransport& value,const SessionRole candidate)
        {
            if(!CanAttach(candidate))return false;ClearNetwork();transport=&value;role=candidate;failure.clear();
            status=role==SessionRole::Host?L"Лобби открыто. Сервер может начать игру до подключения гостя.":L"Подключаемся. Клиент ждёт назначения готового уровня сервером.";return true;
        }
        bool StartHost(const std::uint64_t now)
        {
            if(!transport || role!=SessionRole::Host || shutdown || Active() || !allCompletedCleanup)return false;
            StartGame(SessionRole::Host,now);return true;
        }
        void CompleteGame()
        {
            gameSoftStopAt=0;
            gameSoftLimitReached=false;
            lastResult=game->Result();hasResult=true;++metrics.gameCompletions;
            if(lastResult.successful)++metrics.gameSuccessfulCompletions;
            const bool nativeClean=!nativePending && (!activeRemote || (lastNativeCreated>0 && lastNativeCreated==lastNativeRemoved));
            if(nativeClean)++metrics.gameNativeCleanupCompletions;
            if(!lastResult.outerStarted)++metrics.gameNoOuterLaunchCompletions;
            // The existing owned helper may terminate its own child after a
            // requested stop. Preserve that fact in Result; process cleanup
            // additionally requires actual native Remove ACK/unload evidence.
            const bool cleaned=Cleanup(lastResult) && nativeClean;
            allCompletedCleanup=allCompletedCleanup && cleaned;
            if(!game->Failure().empty())failure=game->Failure();
            ClearNetwork();if(transport)transport->ClosePeer();
            activeRemote.reset();nativePending.reset();desiredRemote.reset();haveScale=haveSample=false;
            status=!lastResult.outerStarted && cleaned?L"Игра не была запущена; транзакция MediaPath не начиналась. Можно повторить запуск.":
                cleaned?L"Игра завершена. Очистка модели, выход процесса и восстановление MediaPath подтверждены.":L"Игра завершена. Полная очистка не подтверждена; сохраните отчёты из Cases.";
            // Retain the complete result rather than treating an empty facade as
            // successful cleanup. Only a subsequent explicit start releases it.
            stopRequested=true;
        }
        void ReadGame(const std::uint64_t now)
        {
            if(!game)return;const bool wasFinished=game->Finished();game->Pump();
            if(game->Finished())
            {if(!wasFinished || metrics.gameCompletions<metrics.gameStarts)CompleteGame();return;}
            if(!game->Failure().empty())
            {failure=game->Failure();desiredRemote.reset();if(protocol){ClearNetwork();if(transport)transport->ClosePeer();}status=L"Ошибка игры; ожидаем завершения и восстановления MediaPath: "+Wide(failure);}
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
            }
            if(game->Ready() && !readyCounted)
            {
                Require(game->Snapshot().levelId==28 && !game->Snapshot().epochRetired,"Only Alfea28 may be assigned in Stage P");
                readyCounted=true;++metrics.gameReady;sceneTransform=game->Snapshot().local;
                for(const auto scale:sceneTransform.scale)Require(std::isfinite(scale) && scale>0,"Qualified local scale is invalid");
                haveScale=true;status=role==SessionRole::Host?L"Alfea 28 готова у сервера. Ожидаем гостя или его загрузку.":L"Alfea 28 готова у клиента. Ожидаем разрешение сервера.";
            }
            ReadNativeAck(now);
        }
        void ReadNativeAck(const std::uint64_t now)
        {
            if(!nativePending || !game)return;const auto& ack=game->Acknowledgement();
            if(ack.sequence<nativePending->sequence)return;
            Require(ack.sequence==nativePending->sequence && ack.entity==nativePending->key.entity && ack.operation==nativePending->operation &&
                ack.session==game->Identity().session && ack.levelEpoch==game->Identity().levelEpoch && ack.status==adapter::RuntimeAckStatus::Applied,
                "Native visual acknowledgement does not match the immutable outstanding command");
            Require(now>=nativePending->began,"Native command clock regressed");const auto elapsed=now-nativePending->began;
            ++metrics.nativeAcknowledgements;metrics.nativeAckLatencyTotalMs+=elapsed;metrics.nativeAckLatencyMaxMs=std::max(metrics.nativeAckLatencyMaxMs,elapsed);
            if(ack.operation==adapter::RuntimeOperation::Create){activeRemote=nativePending->key;appliedRevision=nativePending->revision;++metrics.nativeCreatesAcknowledged;}
            else if(ack.operation==adapter::RuntimeOperation::Apply){appliedRevision=nativePending->revision;++metrics.nativeAppliesAcknowledged;}
            else if(ack.operation==adapter::RuntimeOperation::Remove){activeRemote.reset();appliedRevision=0;++metrics.nativeRemovesAcknowledged;}
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
            nativePending=NativePending{key,operation,game->Result().commandsPublished,now,visual?visual->revision:0};++metrics.nativeCommands;return true;
        }
        void DriveVisual(const std::uint64_t now)
        {
            if(!Active())return;
            if(game->Ready() && haveScale && !nativePending && !game->CommandPending())
            {
                if(activeRemote && (!desiredRemote || !(desiredRemote->key==*activeRemote)))Issue(adapter::RuntimeOperation::Remove,*activeRemote,nullptr,now);
                else if(desiredRemote && !activeRemote)Issue(adapter::RuntimeOperation::Create,desiredRemote->key,&*desiredRemote,now);
                else if(desiredRemote && activeRemote && desiredRemote->revision>appliedRevision)Issue(adapter::RuntimeOperation::Apply,desiredRemote->key,&*desiredRemote,now);
            }
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
                    game->RequestStop();stopRequested=true;status=L"Завершаем игру и ждём подтверждённого восстановления MediaPath…";
                }
            }
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
            status=L"Достигнут лимит прототипа. Удаляем удалённую модель до остановки игры…";
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
            config.compatibility=profile.compatibility;config.qualifiedLevel=profile.level;config.hostSession=role==SessionRole::Host?FreshId():0;
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
                    Require(event->level==profile.level,"Server assigned an unqualified scene");
                    if(role==SessionRole::Guest)
                    {
                        Require(!Active(),"Guest cannot reuse a native generation for a new assignment");
                        StartGame(SessionRole::Guest,now);
                    }
                    break;
                case gameplay::EventKind::Started:++metrics.cooperativeStarts;status=L"Оба игрока готовы. Каждый управляет своей Блум; сервер принимает состояния. P: только Alfea 28.";break;
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
                default:throw std::runtime_error("Retired input event reached the V3 coordinator");
                }
            }
        }
        void Decide(const std::uint64_t now)
        {
            if(shutdown || stopGameAfterRemoval || stopRequested || !protocol || role!=SessionRole::Host || protocol->CurrentPhase()!=gameplay::Phase::Running)return;
            while(!decisions.empty() && protocol->OutboundCount()<gameplay::GameplayProtocol::MaximumOutbound-2 && protocol->EventCount()<gameplay::GameplayProtocol::MaximumEvents-2)
            {
                // V3 policy accepts an exactly identified, structurally checked
                // observation of stock local movement. This is not native host
                // physics, collision validation or an anti-cheat speed test.
                Require(game && Active() && !stopRequested && game->HasRuntimeSnapshot() && !game->Snapshot().epochRetired && game->Snapshot().levelId==28,
                    "Host retired its qualified current level before deciding a guest state");
                if(!game->Ready())
                {
                    // Pause can suspend native readiness without retiring the
                    // scene. Explicit busy decisions consume the FIFO so a long
                    // pause cannot fill the guest receipt window or write IPC.
                    Require(protocol->RejectObservedState(decisions.front().token,2),"Host paused-scene rejection failed");++metrics.observationsRejected;
                }
                else if(decisions.front().withinNumericEnvelope)
                {Require(protocol->AcceptObservedState(decisions.front().token,Next(hostGuestTick)),"Host observation decision failed");++metrics.observationsAccepted;}
                else
                {Require(protocol->RejectObservedState(decisions.front().token,1),"Host bounded-pose rejection failed");++metrics.observationsRejected;}
                decisions.pop_front();
                Events(now);Flush(now);
            }
        }
        void Network(const std::uint64_t now)
        {
            if(!transport || shutdown)return;
            if(Active() && (stopGameAfterRemoval || stopRequested))
            {if(transport->ActiveConnectionId())transport->ClosePeer();return;}
            const auto connection=transport->ActiveConnectionId();const auto* carrier=transport->Session();
            const bool running=connection && carrier && carrier->Phase()==SessionPhase::Running;
            if(protocol && (!running || protocol->ConnectionId()!=connection))
            {
                ClearNetwork();if(gameRole==SessionRole::Guest && Active())stopGameAfterRemoval=true;
                status=role==SessionRole::Host?L"Гость отключён. Игра сервера остаётся открыта; ждём нового подключения.":L"Сервер отключился. Закрываем клиентскую игру и удаляем модель.";
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
            if(role==SessionRole::Host && game && game->Ready() && !stopRequested && !activeRemote && !nativePending && !protocol->LevelEpoch())
                Require(protocol->HostAssignLevel(1,profile.level),"Host failed to assign its actual ready Alfea28 level");
            if(role==SessionRole::Guest && game && game->Ready() && protocol->LevelEpoch() && !protocol->LocalReady())
                Require(protocol->NotifyLocalReady(profile.level),"Guest failed its local-ready barrier");
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
            ReadGame(now);CheckGameLimit(now);Network(now);Sample(now);Flush(now);DriveVisual(now);
            const bool together=protocol && protocol->CurrentPhase()==gameplay::Phase::Running && game && game->Ready() &&
                activeRemote && !stopRequested && !stopGameAfterRemoval && now>=game->Snapshot().monotonicMilliseconds &&
                now-game->Snapshot().monotonicMilliseconds<=1000;
            if(together && lastCooperativePump && now>=lastCooperativePump)metrics.cooperativeRunningMs+=now-lastCooperativePump;
            lastCooperativePump=together?now:0;
        }
        void Shutdown() noexcept
        {
            shutdown=true;ClearNetwork();desiredRemote.reset();stopGameAfterRemoval=true;
            if(!Active()){try{status=L"Игра завершена; закрываем лобби.";}catch(...){}}
        }
        void Fail(const char* message) noexcept
        {
            try{failure=message;status=L"Ошибка кооператива: "+Wide(failure)+L". Ожидаем завершения очистки.";}catch(...){}
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
    bool ReplicationCoordinator::CanStartHostGame() const noexcept{return !implementation_->shutdown && implementation_->transport && implementation_->allCompletedCleanup && implementation_->role==SessionRole::Host && !implementation_->Active();}
    bool ReplicationCoordinator::GameActive() const noexcept{return implementation_->Active();}
    bool ReplicationCoordinator::CanAttach(const SessionRole role) const noexcept{return implementation_->CanAttach(role);}
    const std::wstring& ReplicationCoordinator::Status() const noexcept{return implementation_->status;}
    const std::string& ReplicationCoordinator::Failure() const noexcept{return implementation_->failure;}
    const ReplicationMetrics& ReplicationCoordinator::Metrics() const noexcept{return implementation_->metrics;}
    const GameProcessClientResult& ReplicationCoordinator::LastGameResult() const noexcept{return implementation_->lastResult;}
    bool ReplicationCoordinator::HasGameResult() const noexcept{return implementation_->hasResult;}
    bool ReplicationCoordinator::CleanupConfirmed() const noexcept
    {return !implementation_->Active() && implementation_->metrics.gameCompletions==implementation_->metrics.gameStarts && implementation_->allCompletedCleanup;}
}
