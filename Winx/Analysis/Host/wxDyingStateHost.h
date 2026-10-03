#pragma once
#include "wxCharacterSpeedStateHost.h"

namespace winx::reconstruction
{
    class wxCharacterState;
    // Foreign owner/control, message and manager services. The state owns its
    // +3C byte; an adapter must never maintain a separate copy of that byte.
    class wxDyingStateHost : public wxCharacterSpeedStateHost
    {
    public:
        // PC owner+124->14C / PS2 owner+130->158.
        virtual std::uint32_t DyingOwnerKindForAnalysis(void* owner) = 0;
        // PC owner+224 / PS2 owner+230, read only when kind == 24.
        virtual std::uint8_t DyingOwnerFlagForAnalysis(void* owner) = 0;
        // PC owner+12C virtual +4 / PS2 owner+138 virtual +C.
        // This is a virtual foreign call, not the base state's direct write.
        virtual void ResetDyingControlForAnalysis(void* owner) = 0;
        // PC owner+24 / PS2 owner+24; null skips the foreign delivery call.
        virtual void* DyingNotificationTargetForAnalysis(void* owner) = 0;
        // Original packet [code,0,0,0,source,0,p0,p1].
        virtual void SendDyingNotificationForAnalysis(void* target,
            wxCharacterState& source, std::uint32_t code,
            std::uint32_t payload0, std::uint32_t payload1) = 0;
        // PC4135E0 / PS2115B20; original diagnostic format is supplied intact.
        virtual void DyingDiagnosticForAnalysis(const char* format,
            wxCharacterState& source) = 0;
    };

    class wxBloomDyingStateHost : public wxDyingStateHost
    {
    public:
        // PC owner+284 / PS2 owner+2A0.
        virtual std::uint8_t BloomDyingOwnerFlagForAnalysis(void* owner) = 0;
        // PC owner+124->148 / PS2 owner+130->154; nonzero skips update.
        virtual std::uint32_t BloomDyingUpdateGateForAnalysis(void* owner) = 0;
        // Lazy original manager services remain outside the state component.
        virtual void BloomDyingManager593B00ForAnalysis(
            std::uint32_t value, std::uint32_t mode) = 0;
        virtual void BloomDyingManager593A60ForAnalysis(
            std::uint32_t value, std::uint32_t mode) = 0;
        virtual void BloomDyingManager595450ForAnalysis(
            std::uint32_t value, std::uint32_t mode) = 0;
        virtual void BloomDyingManager5953E0ForAnalysis(
            std::uint32_t value, std::uint32_t mode) = 0;
    };
}
