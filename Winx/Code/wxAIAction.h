#pragma once

// Inferred game-source location. Native layouts and addresses are kept in
// Analysis/{PC,PS2}; this class is a portable behavior reconstruction.

#include "Code/SparkBase/spBaseObject.h"

#include <array>
#include <cstdint>
#include <memory>
#include <vector>

namespace winx::reconstruction
{
    struct wxAIActionMessageForAnalysis final
    {
        std::uint32_t code = 0;
    };

    class wxAIAction : public sparkplug::reconstruction::spBaseObject
    {
    public:
        static constexpr sparkplug::reconstruction::spClassID ClassID =
            0x490A6EB5;

        wxAIAction() noexcept;
        ~wxAIAction() override;

        wxAIAction(const wxAIAction&) = delete;
        wxAIAction& operator=(const wxAIAction&) = delete;

        [[nodiscard]] static const sparkplug::reconstruction::spRTTIRecord&
            StaticRTTI() noexcept;

        void vfunc_0C(const void* notification) noexcept override;
        [[nodiscard]] std::unique_ptr<sparkplug::reconstruction::spBaseObject>
            vfunc_10(sparkplug::reconstruction::spCloneManager& manager) const override;
        bool vfunc_14(sparkplug::reconstruction::spBaseObject& destination,
            sparkplug::reconstruction::spCloneManager& manager) const override;
        [[nodiscard]] const sparkplug::reconstruction::spRTTIRecord&
            vfunc_18() const noexcept override;

        // Analytical PS2 byte offsets. PC uses these slots minus 8 because its
        // vtable has no two-word header. Original source-level names are open.
        [[nodiscard]] virtual bool vfunc_24() noexcept;
        [[nodiscard]] virtual bool vfunc_28() noexcept;
        [[nodiscard]] virtual bool vfunc_2C() noexcept;
        virtual void vfunc_30() noexcept;
        virtual void vfunc_34_ClearForAnalysis() noexcept;
        virtual void vfunc_38() noexcept;
        [[nodiscard]] virtual bool vfunc_3C(const void* argument) noexcept;
        [[nodiscard]] virtual bool vfunc_40(const void* argument) noexcept;

        // Portable test/integration controls. Native ownership is documented
        // separately; these helpers do not claim original names or ABI.
        void SetCurrentActionForAnalysis(wxAIAction* current) noexcept;
        void AddOwnedActionForAnalysis(
            std::unique_ptr<sparkplug::reconstruction::spBaseObject> action);
        void SetOwnerForAnalysis(void* owner) noexcept;
        void SetField24ForAnalysis(void* value) noexcept;
        void SetClearableWordForAnalysis(std::uint32_t value) noexcept;

        [[nodiscard]] wxAIAction* GetCurrentActionForAnalysis() const noexcept;
        [[nodiscard]] std::size_t GetOwnedActionCountForAnalysis() const noexcept;
        [[nodiscard]] void* GetOwnerForAnalysis() const noexcept;
        [[nodiscard]] void* GetField24ForAnalysis() const noexcept;
        [[nodiscard]] const std::array<std::uint32_t, 7>&
            GetZeroWordsForAnalysis() const noexcept;
        [[nodiscard]] std::uint32_t GetDurationMillisecondsForAnalysis() const noexcept;
        [[nodiscard]] std::uint32_t GetClearableWordForAnalysis() const noexcept;
        [[nodiscard]] const std::array<float, 3>& GetVector0ForAnalysis() const noexcept;
        [[nodiscard]] const std::array<float, 3>& GetVector1ForAnalysis() const noexcept;
        [[nodiscard]] float GetScalarBetweenVectorsForAnalysis() const noexcept;
        [[nodiscard]] const std::array<float, 3>& GetVector2ForAnalysis() const noexcept;

    protected:
        void ClearDestinationForAnalysis() noexcept;

    private:
        wxAIAction* currentAction_ = nullptr; // borrowed
        std::vector<std::unique_ptr<sparkplug::reconstruction::spBaseObject>>
            ownedActions_;
        void* owner_ = nullptr;   // borrowed, native +20 PC / +24 PS2
        void* field24_ = nullptr; // borrowed/opaque, native +24 PC / +28 PS2
        std::array<std::uint32_t, 7> zeroWords_{};
        std::uint32_t durationMilliseconds_ = 2000;
        std::uint32_t clearableWord_ = 0;
        std::array<float, 3> vector0_{};
        std::array<float, 3> vector1_{};
        float scalarBetweenVectors_ = 0.0f;
        std::array<float, 3> vector2_{};
    };
}
