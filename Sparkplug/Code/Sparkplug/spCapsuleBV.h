#pragma once
// Inferred path. PC factory4884D0/primary6EC334, PS2 factory129900/primary48D220.
// Native allocation78 and type3. Authored shape and slot1C transform are
// represented here; no native storage ABI or collision-query implementation.
#include "spBoundingVolume.h"

namespace sparkplug::reconstruction
{
    class spCapsuleBV final : public spBoundingVolume
    {
    public:
        static constexpr spClassID ClassID = 0x312FABC0;
        static constexpr unsigned TypeForAnalysis = 3;
        struct StateForAnalysis
        {
            float length = 1;
            float radius = 1;
            Matrix3 orientation{1,0,0, 0,1,0, 0,0,1};
            Vector3 first{0,-0.5F,0};
            Vector3 second{0,0.5F,0};
            Vector3 position{};
        };

        spCapsuleBV() noexcept = default;
        [[nodiscard]] static const spRTTIRecord& StaticRTTI() noexcept;
        [[nodiscard]] const spRTTIRecord& vfunc_18() const noexcept override;
        [[nodiscard]] std::unique_ptr<spBaseObject> vfunc_10(spCloneManager& manager) const override;
        [[nodiscard]] StateForAnalysis GetStateForAnalysis() const noexcept;
        void SetStateForAnalysis(const StateForAnalysis& state) noexcept;
        // Explicit qualification of the finite, nearest-rounded PC arithmetic
        // slice. False preserves caller payload and cached endpoints.
        [[nodiscard]] bool TryUpdateCollisionTransformForAnalysis(Vector3& position,
            Matrix3& orientation, const Vector3& scale) const noexcept override;
        // Compatibility alias has no status channel. Call Try for a finished
        // analytical operation that must report unsupported math explicitly.
        void UpdateCollisionTransformForAnalysis(Vector3& position, Matrix3& orientation,
            const Vector3& scale) const noexcept override;
    private:
        float length_ = 1;
        float radius_ = 1;
        Matrix3 orientation_{1,0,0, 0,1,0, 0,0,1};
        // Native slot1C mutates these two endpoint caches on the original
        // object; the tools' inherited transform contract is const.
        mutable Vector3 first_{0,-0.5F,0};
        mutable Vector3 second_{0,0.5F,0};
        Vector3 position_{};
    };
}
