#pragma once

// Original class identity, inferred source/header placement. PC extent 0x2C;
// these host containers/pointers are not ABI-compatible with the original.
#include "../SparkBase/spBaseObject.h"
#include <memory>
#include <optional>

namespace sparkplug::reconstruction
{
    class spController;
    class spTransformEval;

    class spAnimationManager final : public spBaseObject
    {
        // Host-only lifetime token; not an invented native engine class.
        struct BindingLifetimeForAnalysis
        {
            spAnimationManager* owner;
        };

      public:
        static constexpr spClassID ClassID = 0x5D214CC1;
        struct NameBindingForAnalysis
        {
            std::int32_t slot;
            std::uint32_t references;
        };
        // One owned name reference. Weak manager lifetime prevents dangling
        // Unbind on reversed host teardown; native requires manager outlive it.
        class NameBindingLeaseForAnalysis
        {
          public:
            NameBindingLeaseForAnalysis() = default;
            ~NameBindingLeaseForAnalysis();
            NameBindingLeaseForAnalysis(const NameBindingLeaseForAnalysis&) = delete;
            NameBindingLeaseForAnalysis& operator=(const NameBindingLeaseForAnalysis&) = delete;
            NameBindingLeaseForAnalysis(NameBindingLeaseForAnalysis&&) noexcept;
            NameBindingLeaseForAnalysis& operator=(NameBindingLeaseForAnalysis&&) noexcept;
            [[nodiscard]] std::int32_t GetSlotForAnalysis() const noexcept
            {
                return slot_;
            }
            [[nodiscard]] std::string_view GetNameForAnalysis() const noexcept
            {
                return name_;
            }
            [[nodiscard]] bool BelongsToForAnalysis(const spAnimationManager&) const noexcept;
            void ResetForAnalysis() noexcept;

          private:
            friend class spAnimationManager;
            NameBindingLeaseForAnalysis(std::weak_ptr<BindingLifetimeForAnalysis>, std::string,
                                        std::int32_t) noexcept;
            std::weak_ptr<BindingLifetimeForAnalysis> lifetime_;
            std::string name_;
            std::int32_t slot_ = -1;
        };
        spAnimationManager();
        ~spAnimationManager() override;
        spAnimationManager(const spAnimationManager&) = delete;
        spAnimationManager& operator=(const spAnimationManager&) = delete;
        [[nodiscard]] static const spRTTIRecord& StaticRTTI() noexcept;
        [[nodiscard]] static spAnimationManager* GetInstance() noexcept;
        [[nodiscard]] const spRTTIRecord& vfunc_18() const noexcept override;
        [[nodiscard]] std::unique_ptr<spBaseObject> vfunc_10(spCloneManager&) const override;
        bool vfunc_14(spBaseObject&, spCloneManager&) const override;

        // Original names are case-sensitive C strings. Additional host limits:
        // <=4095 bytes/no embedded NUL, <=4096 names, no slot/refcount overflow.
        [[nodiscard]] std::int32_t BindNameForAnalysis(std::string_view name);
        void UnbindNameForAnalysis(std::string_view name);
        [[nodiscard]] std::optional<NameBindingForAnalysis> FindNameForAnalysis(
            std::string_view name) const;
        [[nodiscard]] std::size_t GetNameCountForAnalysis() const noexcept;
        [[nodiscard]] std::uint32_t GetNextSlotForAnalysis() const noexcept;
        [[nodiscard]] std::optional<NameBindingLeaseForAnalysis> AcquireNameBindingForAnalysis(
            std::string_view name);
        // Corresponds to 0x4545F0 after the controller's evaluator/name getters.
        // Detach only releases the name; it does not reset evaluator +0x10.
        [[nodiscard]] std::int32_t AttachEvaluatorForAnalysis(spTransformEval&, std::string_view);
        void DetachEvaluatorForAnalysis(std::string_view name);

        [[nodiscard]] bool RegisterControllerForAnalysis(spController&) noexcept;
        void UnregisterControllerForAnalysis(spController&) noexcept;
        // Captured spEngineCore +0xB8 is explicit until the outer engine frame
        // caller is reconstructed. Original order/gate and post-call next read.
        [[nodiscard]] bool AdvanceFrameForAnalysis(float capturedDelta);
        [[nodiscard]] std::uint32_t GetFrameForAnalysis() const noexcept;
        [[nodiscard]] std::size_t GetControllerCountForAnalysis() const noexcept;

      private:
        static spAnimationManager* instance_;
        std::uint32_t frame_ = 1, nextSlot_ = 1;
        std::map<std::string, NameBindingForAnalysis, std::less<>> names_;
        std::shared_ptr<BindingLifetimeForAnalysis> bindingLifetime_;
        spController* head_ = nullptr;
        spController* tail_ = nullptr;
        std::size_t controllerCount_ = 0;
        // Host-only protection against reentry/current-controller destruction.
        // No claim that native callbacks permit arbitrary lifetime mutations.
        bool inFrame_ = false;
        spController* frameCursor_ = nullptr;
    };
} // namespace sparkplug::reconstruction
