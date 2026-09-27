#pragma once
#include "Code/SparkBase/spBaseObject.h"
#include "Analysis/Host/wxAudioListenerHost.h"

namespace winx::reconstruction
{
    // Native physical parent wxEntity is represented by mandatory host operations.
    class wxAudioListener : public sparkplug::reconstruction::spNamedObject
    {
    public:
        static constexpr sparkplug::reconstruction::spClassID ClassID = 0x0982734A;
        explicit wxAudioListener(wxAudioListenerHost&);
        ~wxAudioListener() override;
        wxAudioListener(const wxAudioListener&) = delete;
        wxAudioListener& operator=(const wxAudioListener&) = delete;
        static void SetFactoryHostForAnalysis(wxAudioListenerHost*) noexcept;
        static const sparkplug::reconstruction::spRTTIRecord& StaticRTTI() noexcept;
        const sparkplug::reconstruction::spRTTIRecord& vfunc_18() const noexcept override;
        std::unique_ptr<sparkplug::reconstruction::spBaseObject> vfunc_10(
            sparkplug::reconstruction::spCloneManager&) const override;
        bool vfunc_14(sparkplug::reconstruction::spBaseObject&,
            sparkplug::reconstruction::spCloneManager&) const override;
        void vfunc_0C(const void*) noexcept override;
        void InitializeForAnalysis();
        void* ListenerForAnalysis() const noexcept { return listener_; }
        bool IsInitializedForAnalysis() const noexcept { return initialized_; }
    private:
        wxAudioListenerHost& host_;
        void* listener_ = nullptr;
        bool initialized_ = false;
    };
}
