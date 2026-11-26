#pragma once

#include "spSubController.h"

namespace sparkplug::reconstruction
{
    class spPCVideoStream;

    class spMovieTextureController final : public spSubController
    {
      public:
        static constexpr spClassID ClassID = 0x7487689D;

        // The material link and concrete PC video must outlive the controller.
        struct BorrowedPcMaterialForAnalysis
        {
            spPCVideoStream& video;
        };

        explicit spMovieTextureController(BorrowedPcMaterialForAnalysis& material) noexcept;
        [[nodiscard]] static const spRTTIRecord& StaticRTTI() noexcept;
        [[nodiscard]] const spRTTIRecord& vfunc_18() const noexcept override;
        void ApplyForAnalysis(float delta) override;

        // The bool records whether the original slot-00 provider was called.
        [[nodiscard]] bool ApplyAndObserveForAnalysis(float delta) noexcept;

      private:
        const BorrowedPcMaterialForAnalysis& material_;
    };
} // namespace sparkplug::reconstruction
