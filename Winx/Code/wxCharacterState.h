#pragma once

// Inferred game-source location. The registered class names, physical layouts
// and slot bodies documented here are matched on PC and PS2. Original C++
// names for the hooks and external owner/consumer objects remain unknown.

#include "Code/SparkBase/spBaseObject.h"
#include "../Analysis/Host/wxCharacterStateHost.h"

#include <array>
#include <cstdint>
#include <memory>

namespace winx::reconstruction
{
    // The native request is caller-owned. The slots only rewrite its first
    // 32-bit packed key, so no original source type is inferred for it.
    struct wxAnimationRequestForAnalysis final
    {
        std::uint32_t packedKey = 0;
    };

    class wxCharacterState : public sparkplug::reconstruction::spBaseObject
    {
    public:
        static constexpr sparkplug::reconstruction::spClassID ClassID =
            0x44817BC2;

        wxCharacterState() noexcept;
        ~wxCharacterState() override;

        wxCharacterState(const wxCharacterState&) = delete;
        wxCharacterState& operator=(const wxCharacterState&) = delete;

        [[nodiscard]] static const sparkplug::reconstruction::spRTTIRecord&
            StaticRTTI() noexcept;

        [[nodiscard]] std::unique_ptr<sparkplug::reconstruction::spBaseObject>
            vfunc_10(sparkplug::reconstruction::spCloneManager& manager) const override;
        [[nodiscard]] const sparkplug::reconstruction::spRTTIRecord&
            vfunc_18() const noexcept override;

        // Analytical aliases for PC vtable slots 0x1c through 0x40. The
        // original source-level names and argument type are still open.
        // Retain the RTTI overloads, whose analytical names use PS2 offsets.
        using spBaseObject::vfunc_1C;
        using spBaseObject::vfunc_20;
        [[nodiscard]] virtual bool vfunc_1C(
            wxAnimationRequestForAnalysis& request);
        [[nodiscard]] virtual bool vfunc_20(
            wxAnimationRequestForAnalysis& request);
        virtual void vfunc_24();
        virtual void vfunc_28(wxAnimationRequestForAnalysis& request);
        virtual void vfunc_2C(wxAnimationRequestForAnalysis& request);
        virtual void vfunc_30(wxAnimationRequestForAnalysis& request);
        [[nodiscard]] virtual bool vfunc_34(
            const wxAnimationRequestForAnalysis& request) const noexcept;
        [[nodiscard]] virtual bool vfunc_38(
            const wxAnimationRequestForAnalysis& request) const noexcept;
        virtual void vfunc_3C(wxAnimationRequestForAnalysis& request) noexcept;
        virtual void vfunc_40_ResetForAnalysis() noexcept;

        void SetBindingsForAnalysis(void* owner, void* completionConsumer,
            wxCharacterStateHost* host) noexcept;
        void SetTransitionFlagsForAnalysis(bool flag1C, bool flag1D,
            bool flag1E, bool flag1F, bool flag20) noexcept;
        void SetPendingHandleForAnalysis(void* handle) noexcept;

        [[nodiscard]] std::uint32_t GetStateSelectorForAnalysis() const noexcept;
        [[nodiscard]] void* GetPendingHandleForAnalysis() const noexcept;
        [[nodiscard]] std::array<bool, 5> GetTransitionFlagsForAnalysis() const noexcept;

    protected:
        void SetStateSelectorForConstruction(std::uint32_t selector) noexcept;
        // Missing adapters are explicit host errors, never native success.
        [[nodiscard]] wxCharacterStateHost& RequireHostForAnalysis() const;
        [[nodiscard]] void* GetOwnerForAnalysis() const noexcept;
        [[nodiscard]] void* GetCompletionConsumerForAnalysis() const noexcept;
        void SetPendingHandleFromState(void* handle) noexcept;
        void ClearOwnerActionControlFromState();
        void ReleasePendingFromState(bool forceStop = false);
        void QueuePendingFromState(void* handle, bool mode, bool interrupt);

        [[nodiscard]] bool GetTransitionFlag1C() const noexcept;
        [[nodiscard]] bool GetTransitionFlag1E() const noexcept;
        [[nodiscard]] bool GetTransitionFlag1F() const noexcept;
        [[nodiscard]] bool GetTransitionFlag20() const noexcept;
        void ClearTransitionFlag1C() noexcept;
        void ClearTransitionFlag1E() noexcept;

    private:
        // This portable object intentionally does not claim the native ABI.
        // Exact 32-bit layouts live in Analysis/{PC,PS2}.
        std::uint32_t stateSelector_ = 0;
        void* owner_ = nullptr;
        void* completionConsumer_ = nullptr;
        bool transitionFlag1C_ = true;
        bool transitionFlag1D_ = true;
        bool transitionFlag1E_ = true;
        bool transitionFlag1F_ = true;
        bool transitionFlag20_ = true;
        void* pendingHandle_ = nullptr;
        void* field28_ = nullptr;
        bool field2C_ = false;
        std::array<float, 3> resetValues_{};
        wxCharacterStateHost* host_ = nullptr;
    };
}
