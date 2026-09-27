#include "wxAudioEmitter.h"
#include <stdexcept>

namespace winx::reconstruction
{
    using namespace sparkplug::reconstruction;
    namespace
    {
        wxAudioEmitterHost* factoryHost = nullptr;
        std::unique_ptr<spBaseObject> CreateAudioEmitter()
        {
            if (!factoryHost) throw std::logic_error("wxAudioEmitter requires a factory host");
            return std::make_unique<wxAudioEmitter>(*factoryHost);
        }
        const spRTTIRecord entityRecord{0x22875AA1, spNamedObject::ClassID,
            "spEntity", &spNamedObject::StaticRTTI(), nullptr, nullptr};
        const spRTTIRecord wxEntityRecord{0x796A1869, 0x22875AA1,
            "wxEntity", &entityRecord, nullptr, nullptr};
        const spRTTIRecord record{wxAudioEmitter::ClassID, 0x796A1869,
            "wxAudioEmitter", &wxEntityRecord, &CreateAudioEmitter, nullptr};
        const bool registered = spRTTIManager::Instance().RegisterDeferredForAnalysis(record);
    }

    wxAudioEmitter::wxAudioEmitter(wxAudioEmitterHost& host) : host_(host)
    {
        host_.ConstructEntityForAnalysis(*this, true);
        host_.ConstructAudioStateForAnalysis(*this);
    }
    wxAudioEmitter::~wxAudioEmitter()
    {
        host_.DestroyAudioStateForAnalysis(*this);
        host_.DestroyEntityForAnalysis(*this);
    }
    void wxAudioEmitter::SetFactoryHostForAnalysis(wxAudioEmitterHost* host) noexcept { factoryHost = host; }
    const spRTTIRecord& wxAudioEmitter::StaticRTTI() noexcept { (void)registered; return record; }
    const spRTTIRecord& wxAudioEmitter::vfunc_18() const noexcept { return record; }
    std::unique_ptr<spBaseObject> wxAudioEmitter::vfunc_10(spCloneManager& manager) const
    {
        auto copy = std::make_unique<wxAudioEmitter>(host_);
        manager.RegisterCloneForAnalysis(*this, *copy);
        if (!vfunc_14(*copy, manager)) return nullptr;
        return copy;
    }
    bool wxAudioEmitter::vfunc_14(spBaseObject& destination, spCloneManager& manager) const
    {
        auto* target = dynamic_cast<wxAudioEmitter*>(&destination);
        if (!target) return false; // portable type guard; native assumes a valid receiver
        if (!host_.CopyEntityForAnalysis(*this, *target, manager)) return false;
        host_.CopyOwnedResourceForAnalysis(*this, *target);
        target->state_.flag5D = state_.flag5D;
        for (unsigned i = 0; i != 3; ++i) target->state_.words[i] = state_.words[i];
        return true;
    }
    void wxAudioEmitter::vfunc_0C(const void* message) noexcept
    {
        switch (*static_cast<const std::uint32_t*>(message))
        {
        case 0x1C: host_.InitializeForAnalysis(*this); break;
        case 0x1E: host_.HandleCode30ForAnalysis(*this); break;
        case 0x2711:
            if (state_.flag5D) host_.SetAudioActiveForAnalysis(*this, true);
            break;
        default: host_.HandleOtherNotificationForAnalysis(*this, message); break;
        }
    }
    bool wxAudioEmitter::CompareKeysForAnalysis(const char* leftName, std::int32_t leftVariant,
        const char* rightName, std::int32_t rightVariant) noexcept
    {
        const auto* left = reinterpret_cast<const unsigned char*>(leftName);
        const auto* right = reinterpret_cast<const unsigned char*>(rightName);
        while (*left && *left == *right) { ++left; ++right; }
        if (*left != *right) return *left < *right;
        return leftVariant < rightVariant && leftVariant != 0 && rightVariant != 0
            && leftVariant != 10 && rightVariant != 10;
    }
    void wxAudioEmitter::DispatchTagForAnalysis(const char* name, std::uint32_t variant)
    {
        const auto gameState = host_.GameStateForAnalysis();
        if (!(gameState <= 37 || (gameState >= 41 && gameState <= 49) || gameState == 70)) return;
        auto* entry = host_.FindEntryForAnalysis(*this, name, variant, 1000000);
        if (!entry) return;
        if (entry->kind == 10)
        {
            host_.PlayKind10ForAnalysis(entry->sounds.front());
            return;
        }
        const auto count = static_cast<std::uint32_t>(entry->sounds.size());
        // Original divides by count without an empty-vector check. A native
        // entry reached here must contain at least one sound.
        auto index = host_.RandomForAnalysis() % count;
        if (count > 1)
        {
            if (index == entry->previousIndex && ++index == count) index = 0;
            entry->previousIndex = index;
        }
        host_.PlaySoundForAnalysis(entry->sounds[index]);
    }
}
