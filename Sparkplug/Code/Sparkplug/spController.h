#pragma once

// Inferred paths, original class identity. PC spSubController physical base,
// extent 0x1C. 0x00423010 resolves to 0x004C4210: enabled10, links14/18.
// Native constructor/destructor requires global spAnimationManager. The host
// permits standalone controllers when no manager exists; it never creates one
// implicitly. With a manager present, ctor/dtor register/unregister normally.
#include "spSubController.h"

namespace sparkplug::reconstruction
{
    class spAnimationManager;
    class spController : public spSubController
    {
      public:
        static constexpr spClassID ClassID = 0x4FAD24F1;
        spController();
        ~spController() override;
        spController(const spController&) = delete;
        spController& operator=(const spController&) = delete;
        [[nodiscard]] static const spRTTIRecord& StaticRTTI() noexcept;
        [[nodiscard]] const spRTTIRecord& vfunc_18() const noexcept override;
        bool vfunc_14(spBaseObject&, spCloneManager&) const override;
        [[nodiscard]] bool IsEnabledForAnalysis() const noexcept
        {
            return enabled_;
        }
        void SetEnabledForAnalysis(bool enabled) noexcept
        {
            enabled_ = enabled;
        }

      protected:
        [[nodiscard]] spAnimationManager* GetRegisteredManagerForAnalysis() const noexcept
        {
            return manager_;
        }
      private:
        friend class spAnimationManager;
        bool enabled_ = true; // manager gate, distinct from actor +0x1C
        spController* next_ = nullptr;
        spController* previous_ = nullptr;
        spAnimationManager* manager_ = nullptr; // host-only lifetime guard
    };
} // namespace sparkplug::reconstruction
