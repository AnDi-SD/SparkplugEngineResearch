#pragma once

// Exact source translation-unit path is recovered as
// Z:\Sparkplug\Code\SparkplugPC\spPCApp.cpp.  This header path and the names
// used for unknown slots/fields are inferred.

#include "../SparkBase/spApp.h"

#include <cstddef>
#include <cstdint>
#include <memory>

namespace sparkplug::reconstruction
{
    class spPCApp : public spApp
    {
    public:
        static constexpr spClassID ClassID = 0x7635EFDE;

        enum class PumpResult
        {
            NoMessage,
            DispatchedMessage,
            Quit,
        };

        using MessagePumpForAnalysis = PumpResult (*)(void* context);

        spPCApp() noexcept;
        ~spPCApp() override;

        [[nodiscard]] static const spRTTIRecord& StaticRTTI() noexcept;

        [[nodiscard]] std::unique_ptr<spBaseObject> vfunc_10(
            spCloneManager& manager) const override;
        [[nodiscard]] const spRTTIRecord& vfunc_18() const noexcept override;

        [[nodiscard]] bool vfunc_20_Initialize() override;
        [[nodiscard]] bool vfunc_24_Update() override;
        void vfunc_28_Shutdown() override;
        [[nodiscard]] bool vfunc_2C_Run() override;

        // Safe replacement for the native Win32 PeekMessage loop in tests.
        // Without a backend Run refuses to enter an unbounded loop.
        void SetMessagePumpForAnalysis(
            MessagePumpForAnalysis pump,
            void* context) noexcept;

        [[nodiscard]] std::int32_t GetWindowWidthForAnalysis() const noexcept;
        [[nodiscard]] std::int32_t GetWindowHeightForAnalysis() const noexcept;
        [[nodiscard]] std::uint32_t GetWindowStyleForAnalysis() const noexcept;

    protected:
        // First derived hook called on every base update.  Native spPCApp uses
        // a no-op implementation at 0x0048EAA0.
        virtual void vfunc_30_PreUpdate() noexcept;

    private:
        MessagePumpForAnalysis messagePump_ = nullptr;
        void* messagePumpContext_ = nullptr;
        std::uintptr_t moduleHandle_ = 0;
        std::uintptr_t windowHandle_ = 0;
        std::int32_t windowX_ = 0;
        std::int32_t windowY_ = 0;
        std::int32_t windowWidth_ = 0x400;
        std::int32_t windowHeight_ = 0x300;
        std::uint32_t windowStyle_ = 0x00CA0000;
    };
}
