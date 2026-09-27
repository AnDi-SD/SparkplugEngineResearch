#include "wxAudioListener.h"
#include <stdexcept>

namespace winx::reconstruction
{
    using namespace sparkplug::reconstruction;
    namespace
    {
        wxAudioListenerHost* factoryHost = nullptr;
        std::unique_ptr<spBaseObject> CreateAudioListener()
        {
            if (!factoryHost) throw std::logic_error("wxAudioListener requires a factory host");
            return std::make_unique<wxAudioListener>(*factoryHost);
        }
        const spRTTIRecord entityRecord{0x22875AA1, spNamedObject::ClassID,
            "spEntity", &spNamedObject::StaticRTTI(), nullptr, nullptr};
        const spRTTIRecord wxEntityRecord{0x796A1869, 0x22875AA1,
            "wxEntity", &entityRecord, nullptr, nullptr};
        const spRTTIRecord record{wxAudioListener::ClassID, 0x796A1869,
            "wxAudioListener", &wxEntityRecord, &CreateAudioListener, nullptr};
        const bool registered = spRTTIManager::Instance().RegisterDeferredForAnalysis(record);
    }

    wxAudioListener::wxAudioListener(wxAudioListenerHost& host) : host_(host)
    {
        host_.ConstructEntityForAnalysis(*this, true);
        host_.ConstructContainerForAnalysis(*this);
        listener_ = host_.AcquireListenerForAnalysis(*this);
    }
    wxAudioListener::~wxAudioListener()
    {
        if (!host_.IsAudioTeardownForAnalysis())
        {
            host_.StopListenerForAnalysis(*this, listener_);
            host_.UnregisterListenerForAnalysis(*this, listener_);
            if (host_.HasLinkedEntityForAnalysis(*this)) host_.UnlinkEntityForAnalysis(*this);
        }
        host_.DestroyContainerForAnalysis(*this);
        host_.ReleaseListenerForAnalysis(*this, listener_);
        host_.DestroyEntityForAnalysis(*this);
    }
    void wxAudioListener::SetFactoryHostForAnalysis(wxAudioListenerHost* host) noexcept { factoryHost = host; }
    const spRTTIRecord& wxAudioListener::StaticRTTI() noexcept { (void)registered; return record; }
    const spRTTIRecord& wxAudioListener::vfunc_18() const noexcept { return record; }
    std::unique_ptr<spBaseObject> wxAudioListener::vfunc_10(spCloneManager& manager) const
    {
        auto copy = std::make_unique<wxAudioListener>(host_);
        manager.RegisterCloneForAnalysis(*this, *copy);
        if (!vfunc_14(*copy, manager)) return nullptr;
        return copy;
    }
    bool wxAudioListener::vfunc_14(spBaseObject& destination, spCloneManager& manager) const
    {
        auto* target = dynamic_cast<wxAudioListener*>(&destination);
        if (!target) return false; // portable type guard; native expects a valid receiver
        return host_.CopyEntityForAnalysis(*this, *target, manager);
    }
    void wxAudioListener::vfunc_0C(const void* message) noexcept
    {
        if (*static_cast<const std::uint32_t*>(message) == 0x1C)
        {
            // Native call path has no exception handling.
            try { InitializeForAnalysis(); } catch (...) { std::terminate(); }
        }
    }
    void wxAudioListener::InitializeForAnalysis()
    {
        if (initialized_) return;
        host_.AttachEntityForAnalysis(*this);
        host_.RegisterListenerForAnalysis(*this, listener_);
        host_.ActivateListenerForAnalysis(*this, listener_);
        initialized_ = true;
    }
}
