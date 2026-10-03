#pragma once
// Inferred path and analytical API names. PC component; portable C++ storage
// is deliberately independent of the original460-byte layout.
#include "../SparkBase/spBaseObject.h"
#include "../../Analysis/Host/spNetworkManagerHost.h"
#include <array>
#include <deque>
#include <optional>

namespace sparkplug::reconstruction
{
    class spNetworkManager : public spBaseObject
    {
    public:
        using Handle=spNetworkManagerHandleForAnalysis;
        static constexpr spClassID ClassID=0x0546DEC1;
        struct StateForAnalysis
        {
            std::uint8_t started=0,word15=0,forwardNotifications=0;
            std::uint32_t error=0,bias=0;
            std::uint16_t uniqueId=0;
            std::size_t queued=0;
            std::array<bool,8> services{}; // server,peer,matchmaking,debug,controller,stats,error lock,timer
            std::vector<std::uint32_t> statistics;
        };
        explicit spNetworkManager(std::shared_ptr<spNetworkManagerHost> host=GetNetworkManagerFactoryHostForAnalysis());
        ~spNetworkManager() override;
        spNetworkManager(const spNetworkManager&)=delete;
        spNetworkManager& operator=(const spNetworkManager&)=delete;
        [[nodiscard]] static const spRTTIRecord& StaticRTTI() noexcept;
        [[nodiscard]] const spRTTIRecord& vfunc_18() const noexcept override;
        [[nodiscard]] std::unique_ptr<spBaseObject> vfunc_10(spCloneManager&) const override;
        void vfunc_0C(const void*) noexcept override;
        // Original Copy remains the root successful no-op.
        virtual bool StartForAnalysis();
        virtual void StopForAnalysis() noexcept;
        void SetErrorForAnalysis(std::int32_t);
        void TransitionForAnalysis(std::uint32_t,std::uint32_t);
        void RefreshTimerForAnalysis();
        bool StartConnectionForAnalysis();
        bool SendProbeForAnalysis();
        bool SendHelloRequestForAnalysis();
        bool HandleIdReplyForAnalysis();
        bool HandleHelloReplyForAnalysis();
        bool BroadcastPacketForAnalysis();
        bool PumpForAnalysis();
        bool DispatchInputForAnalysis();
        void SetRecipientForAnalysis(Handle);
        [[nodiscard]] StateForAnalysis GetStateForAnalysis() const;
        [[nodiscard]] static spNetworkManager* CurrentForAnalysis() noexcept;
        [[nodiscard]] static spNetworkManager& GetOrCreateForAnalysis();
        // Our explicit owner for native lazy-factory allocations. Call when
        // their borrowed users have finished; construction never deletes them.
        static void ReleaseLazyStorageForAnalysis() noexcept;
        // Explicit foreign graph/producer setup; no original setter names inferred.
        void EnqueueForAnalysis(Handle packet);
        void SetCurrentPacketForAnalysis(Handle packet) noexcept {currentPacket_=packet;}
        void SetControlsForAnalysis(std::uint8_t started,std::uint8_t word15,std::uint8_t forward) noexcept;
        void SetStatisticForAnalysis(std::size_t index,std::uint32_t value);
        static void SetLatencySamplesForAnalysis(const std::array<std::uint32_t,5>&,std::uint8_t next);
    private:
        void ClearError();
        void Log(std::int32_t,const std::string&,const std::vector<std::uint32_t>& arguments={});
        std::uint32_t TimerMilliseconds();
        Handle PopPacket();
        void Increment(std::size_t);
        const spNetworkManagerPacketForAnalysis& Packet();
        std::shared_ptr<spNetworkManagerHost> host_;
        StateForAnalysis state_;
        Handle server_=0,peer_=0,matchmaking_=0,debug_=0,controller_=0,timer_=0;
        Handle queueLock_=0,errorLock_=0,currentPacket_=0,recipient_=0;
        std::deque<Handle> queue_;
        std::optional<std::array<std::uint32_t,28>> statistics_;
        // Constructor-initialized ancillary fields; types/uses still unknown.
        std::uint8_t unknown38_=0,unknown54_=0;
        std::array<std::uint32_t,6> words3C_{};
        std::array<std::uint32_t,6> words58_{};
        std::uint32_t unknown1A8_=0;
        std::array<std::string,10> errors_;
        std::string text1B0_;
    };
}
