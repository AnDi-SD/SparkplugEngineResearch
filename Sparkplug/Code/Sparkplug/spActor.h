#pragma once

// Original class, inferred paths. PC tick, descendant discovery, name/input
// binding, start/stop and normal lifetime. This is not the complete actor API,
// event dispatcher/reentry contract or outer engine/render frame.
#include "spController.h"
#include "spAnimation.h"
#include "spTransformTrackEval.h"
#include "spNodeController.h"
#include <functional>
#include <optional>
#include <string>

namespace sparkplug::reconstruction
{
    class spActor final : public spController
    {
      public:
        static constexpr spClassID ClassID = 0x19D676E6;
        static constexpr std::size_t DefaultPlaybackCapacityForAnalysis = 40;
        struct PlaybackStateForAnalysis
        {
            const spAnimation* animation = nullptr; // borrowed, no native destructor Release
            std::uint32_t mode = 0;
            bool reverse = false;
            float weight = 0;
            std::uint32_t fadeMode = 0;
            float fadeInRate = 0, fadeOutRate = 0, transitionDuration = 0;
            bool hasLoopCallback = false; // recorded, not invoked/reentered
            std::uintptr_t callbackCookie = 0;
            float timeMultiplier = 0, sampleTime = 0;
            bool stopAfterFade = false;
            std::uint32_t status = 0, slotIndex = 0, bindingUseCount = 0;
            bool running = false;
            std::uint32_t priority = 0;
            float normalizedProgress = 0, fadeThreshold = 0, elapsedTime = 0;
            spTransformTrackEval::PlaybackForAnalysis evaluation{0, 0};
        };
        enum class ActionKindForAnalysis
        {
            Event,
            LoopCallback,
            ControllerLookup,
            Direct,
            Blend,
            Rebind,
            Flush,
            ImmediateEvent
        };
        enum class EventPayloadForAnalysis
        {
            Animation,
            Playback,
            Tag
        };
        struct ActionForAnalysis
        {
            ActionKindForAnalysis kind;
            std::size_t playbackIndex = 0;
            std::uint32_t eventCode = 0;
            std::optional<std::uint32_t> tagOrdinal;
            float time = 0, factor = 0;
            EventPayloadForAnalysis payload = EventPayloadForAnalysis::Animation;
        };
        struct ControllerBindingForAnalysis
        {
            std::optional<std::size_t> firstPlayback;
            std::function<void(float)> apply;
            std::function<void(float, float)> blend;
            std::function<std::optional<std::size_t>()> firstPlaybackQuery;
        };
        struct StartRequestForAnalysis
        {
            const spAnimation* animation = nullptr; // borrowed; caller keeps resource alive
            std::uint32_t mode = 1;
            bool reverse = false;
            float weight = 1;
            std::uint32_t fadeMode = 0;
            float fadeInDuration = 0, fallbackFadeInRate = 0;
            float fadeOutDuration = 0, fallbackFadeOutRate = 0;
            float transitionDuration = 0;
            bool hasLoopCallback = false;
            std::uintptr_t callbackCookie = 0;
            float timeMultiplier = 1, initialTime = 0;
        };
        spActor();
        ~spActor() override;
        // PC global741654 starts at40, game init writes2, and a separate
        // caller resets an existing actor to19. Names are analytical.
        [[nodiscard]] static bool SetDefaultPlaybackCapacityForAnalysis(std::size_t) noexcept;
        [[nodiscard]] static std::size_t GetDefaultPlaybackCapacityForAnalysis() noexcept;
        // Host guard: no bound/manual controllers, active states or retained
        // external playback views when replacing the native state storage.
        [[nodiscard]] bool ResetPlaybackCapacityForAnalysis(std::size_t,std::string* error=nullptr);
        [[nodiscard]] bool FadeOutAndStopForAnalysis(const spAnimation*,float duration,float fallbackRate,
                                                    std::string* error=nullptr);
        [[nodiscard]] const PlaybackStateForAnalysis* FindPlaybackForAnalysis(const spAnimation*) const noexcept;
        [[nodiscard]] bool HasUsedAnimationForAnalysis(const spAnimation*) const noexcept;
        [[nodiscard]] static const spRTTIRecord& StaticRTTI() noexcept;
        [[nodiscard]] const spRTTIRecord& vfunc_18() const noexcept override;
        [[nodiscard]] std::unique_ptr<spBaseObject> vfunc_10(spCloneManager&) const override;
        void ApplyForAnalysis(float deltaTime) override;
        [[nodiscard]] bool TickForAnalysis(float deltaTime, std::vector<ActionForAnalysis>& actions,
                                           std::string* error = nullptr);
        [[nodiscard]] PlaybackStateForAnalysis* GetPlaybackForAnalysis(std::size_t index) noexcept;
        [[nodiscard]] std::size_t GetPlaybackCapacityForAnalysis() const noexcept
        {
            return states_.size();
        }
        // Explicit borrowed controller/registry seam; caller keeps captured objects
        // alive. Native event callbacks are recorded, not executed by this slice.
        [[nodiscard]] bool SetControllerBindingsForAnalysis(
            std::vector<ControllerBindingForAnalysis> bindings);
        // Original discovery/tree wrapper. The latter intentionally skips root.
        // Native intrusive node ownership is represented by shared_ptr. Partial
        // tree discovery may remain if a later allocation/binding fails.
        [[nodiscard]] bool DiscoverNodeForAnalysis(std::shared_ptr<spNode>,
                                                   std::string* error = nullptr);
        [[nodiscard]] bool BindDescendantsForAnalysis(spNode& root, std::string* error = nullptr);
        [[nodiscard]] std::size_t GetOwnedControllerCountForAnalysis() const noexcept;
        [[nodiscard]] spNodeController* GetOwnedControllerForAnalysis(std::size_t index) noexcept;
        // Start mutates request.weight on success, as native does. Host failure
        // preflights the complete fixed-two-input plan before publishing it.
        // nullopt with empty error is native no-free-slot, not a thrown failure.
        [[nodiscard]] std::optional<std::size_t> StartForAnalysis(StartRequestForAnalysis&,
                                                                  std::vector<ActionForAnalysis>&,
                                                                  std::string* error = nullptr);
        [[nodiscard]] bool RebindForAnalysis(std::optional<std::size_t> selected,
                                             std::vector<ActionForAnalysis>&,
                                             std::string* error = nullptr);
        [[nodiscard]] bool StopForAnalysis(const spAnimation*, bool suppressEvent,
                                           std::vector<ActionForAnalysis>&,
                                           std::string* error = nullptr);
        [[nodiscard]] bool StopAllForAnalysis(std::vector<ActionForAnalysis>&,
                                              std::string* error = nullptr);
        void SetAppliesTransformsForAnalysis(bool value) noexcept
        {
            applies_ = value;
        }
        void SetAdvancesWhileDisabledForAnalysis(bool value) noexcept
        {
            advances_ = value;
        }
        void SetTimeMultiplierForAnalysis(float value) noexcept
        {
            multiplier_ = value;
        }

      private:
        static std::size_t defaultPlaybackCapacity_;
        bool applies_ = true, advances_ = true;
        float multiplier_ = 1;
        std::vector<PlaybackStateForAnalysis> states_;
        std::vector<ControllerBindingForAnalysis> controllers_;
        struct OwnedControllerForAnalysis
        {
            std::unique_ptr<spNodeController> controller;
            spAnimationManager::NameBindingLeaseForAnalysis name;
        };
        bool ownedBindingMode_ = false;
        std::vector<OwnedControllerForAnalysis> ownedControllers_;
        std::map<std::int32_t, std::size_t> slotControllers_;
        // Stable callable objects; snapshots retain prepared keys, not resources.
        std::map<const spAnimTrack*, spTransformTrackEval::TrackSamplerForAnalysis> samplers_;
        [[nodiscard]] std::optional<std::size_t> PlaybackIndexForAnalysis(
            const spTransformTrackEval::PlaybackForAnalysis*) const noexcept;
        [[nodiscard]] bool BuildBindingPlanForAnalysis(
            std::vector<PlaybackStateForAnalysis>& working,
            std::vector<spTransformTrackEval>& planned, std::optional<std::size_t> selected,
            std::vector<ActionForAnalysis>& actions, std::string* error);
        void CommitBindingPlanForAnalysis(std::vector<PlaybackStateForAnalysis>&,
                                          std::vector<spTransformTrackEval>&) noexcept;
    };
} // namespace sparkplug::reconstruction
