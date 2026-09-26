#pragma once
#include "Analysis/Host/wxAnimationControllerHost.h"

namespace winx::reconstruction
{
    // Physical native parent is wxEntity. Missing entity levels are an explicit
    // host boundary; portable spNamedObject supplies only the existing base API.
    class wxAnimationController : public sparkplug::reconstruction::spNamedObject
    {
    public:
        static constexpr sparkplug::reconstruction::spClassID ClassID = 0xCD2B2876;
        using Animation = sparkplug::reconstruction::spAnimation;
        using Request = sparkplug::reconstruction::spActor::StartRequestForAnalysis;
        struct StateForAnalysis
        {
            void* entity = nullptr; // inherited PC24, not owned by this leaf
            void* character = nullptr;
            void* actor = nullptr; // owned; destroyed through host
            Request request;
            const Animation* mark9 = nullptr;
            const Animation* mark3 = nullptr;
            const Animation* old = nullptr;
            const Animation* recent = nullptr;
            std::uint32_t count = 0;
            std::uint8_t forceEnable = 1;
        };
        explicit wxAnimationController(wxAnimationControllerHost&);
        ~wxAnimationController() override;
        wxAnimationController(const wxAnimationController&) = delete;
        wxAnimationController& operator=(const wxAnimationController&) = delete;
        static void SetFactoryHostForAnalysis(wxAnimationControllerHost*) noexcept;
        static const sparkplug::reconstruction::spRTTIRecord& StaticRTTI() noexcept;
        const sparkplug::reconstruction::spRTTIRecord& vfunc_18() const noexcept override;
        std::unique_ptr<sparkplug::reconstruction::spBaseObject> vfunc_10(sparkplug::reconstruction::spCloneManager&) const override;
        bool vfunc_14(sparkplug::reconstruction::spBaseObject&, sparkplug::reconstruction::spCloneManager&) const override;
        void vfunc_0C(const void*) noexcept override;
        void InitializeForAnalysis();
        void ReleaseActorForAnalysis() noexcept;
        std::uint32_t StartForAnalysis(const Animation*, std::uint32_t mode, std::uint32_t fadeMode, std::uint8_t interruptPrevious);
        void StopForAnalysis(const Animation*);
        void StopAllForAnalysis();
        void FadeStopForAnalysis(const Animation*, float duration);
        void RestartReverseForAnalysis(const Animation*);
        void SetTimeMultiplierForAnalysis(float);
        bool HasMarkedForAnalysis(const Animation*, std::uint8_t consume) noexcept;
        void ClearMarksForAnalysis(const Animation*) noexcept;
        bool vfunc_34_UpdateActorFlagsForAnalysis(const void* unused = nullptr) noexcept;
        StateForAnalysis& GetStateForAnalysis() noexcept { return state_; }
        const StateForAnalysis& GetStateForAnalysis() const noexcept { return state_; }
    private:
        void MarkForAnalysis(const Animation*, bool code3) noexcept;
        wxAnimationControllerHost& host_;
        StateForAnalysis state_;
    };
}
