#pragma once

#include "Code/SparkBase/spBaseObject.h"
#include "Analysis/Host/wxAIManagerHost.h"

#include <list>

namespace winx::reconstruction
{
    // Portable message view; not the original 32-bit notification layout.
    struct wxAIManagerMessageForAnalysis final
    {
        std::uint32_t code;
        std::uint32_t value; // native message +18
    };

    class wxAIManager : public sparkplug::reconstruction::spBaseObject
    {
    public:
        static constexpr sparkplug::reconstruction::spClassID ClassID = 0x56887B3F;
        explicit wxAIManager(wxAIManagerHost& host) noexcept;
        ~wxAIManager() override;
        wxAIManager(const wxAIManager&) = delete;
        wxAIManager& operator=(const wxAIManager&) = delete;

        // Host-only binding needed by the argumentless RTTI factory. A missing
        // binding is an error, not a fabricated game singleton/context.
        static void SetFactoryHostForAnalysis(wxAIManagerHost*) noexcept;
        static wxAIManager* GetInstanceForAnalysis() noexcept;
        static const sparkplug::reconstruction::spRTTIRecord& StaticRTTI() noexcept;
        const sparkplug::reconstruction::spRTTIRecord& vfunc_18() const noexcept override;
        std::unique_ptr<sparkplug::reconstruction::spBaseObject> vfunc_10(
            sparkplug::reconstruction::spCloneManager&) const override;
        // Copy is inherited unchanged: native spBaseObject Copy is a no-op.
        void vfunc_0C(const void*) noexcept override;

        void AddMemberForAnalysis(void*);   // adopts; duplicates are ignored
        void RemoveMemberForAnalysis(void*) noexcept; // detaches; does not delete
        void ClearForAnalysis() noexcept;
        void ProcessThreeForAnalysis() noexcept;
        bool UpdateForAnalysis() noexcept;
        void BroadcastFlagForAnalysis(std::uint32_t) noexcept;
        void ResetLevelForAnalysis(std::uint32_t) noexcept;
        void HandleFallTimerForAnalysis(std::uint32_t) noexcept;
        void CountDoorPair03And05ForAnalysis() noexcept;
        void CountDoorPair01And06ForAnalysis() noexcept;
        void CountLevel21ForAnalysis() noexcept;
        void CountLevel18ForAnalysis() noexcept;
        void SetBattleCageForAnalysis(bool) noexcept;

        struct StateForAnalysis final
        {
            std::uint32_t counter28 = 0, counter2C = 0;
            bool triggered30 = false;
            std::uint32_t counter34 = 0, counter38 = 0;
            void* field3C = nullptr;
            void* respawnPoint = nullptr;
            bool pending44 = false;
        };
        const StateForAnalysis& GetStateForAnalysis() const noexcept { return state_; }
        std::size_t GetMemberCountForAnalysis() const noexcept { return members_.size(); }
        // Observational indices; size() represents the native end sentinel.
        std::size_t GetCursor20ForAnalysis() const noexcept;
        std::size_t GetCursor24ForAnalysis() const noexcept;

    private:
        void UpdateLevel14ForAnalysis() noexcept;
        void UpdateLevel6ForAnalysis() noexcept;
        void OpenDoorPairForAnalysis(const char*, const char*) noexcept;
        wxAIManagerHost& host_;
        bool cleared_ = false; // host guard: native Clear can leave dangling cursors
        std::list<void*> members_; // PC order and ownership, not native ABI
        std::list<void*>::iterator cursor20_, cursor24_;
        StateForAnalysis state_;
    };
}
