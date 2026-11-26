#include "spMovieTextureController.h"
#include "../SparkplugPC/spPCVideoStream.h"

namespace sparkplug::reconstruction
{
    spMovieTextureController::spMovieTextureController(BorrowedPcMaterialForAnalysis& material) noexcept
        : material_(material)
    {
    }

    const spRTTIRecord& spMovieTextureController::StaticRTTI() noexcept
    {
        static const spRTTIRecord record{ClassID, spSubController::ClassID, "spMovieTextureController",
                                        &spSubController::StaticRTTI(), nullptr, nullptr};
        return record;
    }

    const spRTTIRecord& spMovieTextureController::vfunc_18() const noexcept
    {
        return StaticRTTI();
    }

    void spMovieTextureController::ApplyForAnalysis(float delta)
    {
        (void)ApplyAndObserveForAnalysis(delta);
    }

    bool spMovieTextureController::ApplyAndObserveForAnalysis(float) noexcept
    {
        auto& video = material_.video;
        if (video.GetStateForAnalysis().byte18 != 0)
            return false;

        (void)video.NativeHook00ForAnalysis();
        return true;
    }
} // namespace sparkplug::reconstruction
