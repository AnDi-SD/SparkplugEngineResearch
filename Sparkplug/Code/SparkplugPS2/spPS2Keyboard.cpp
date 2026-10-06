#include "spPS2Keyboard.h"

namespace sparkplug::reconstruction
{
    namespace
    {
        std::unique_ptr<spBaseObject> CreateKeyboard()
        { return std::make_unique<spPS2Keyboard>(); }
        const spRTTIRecord KeyboardRecord{spPS2Keyboard::ClassID, spPS2InputDevice::ClassID,
            "spPS2Keyboard", &spPS2InputDevice::StaticRTTI(), &CreateKeyboard, nullptr};
        const bool KeyboardRegistered = spRTTIManager::Instance().RegisterDeferredForAnalysis(KeyboardRecord);
    }
    const spRTTIRecord& spPS2Keyboard::StaticRTTI() noexcept
    { (void)KeyboardRegistered; return KeyboardRecord; }
    const spRTTIRecord& spPS2Keyboard::vfunc_18() const noexcept { return KeyboardRecord; }
    std::unique_ptr<spBaseObject> spPS2Keyboard::vfunc_10(spCloneManager& manager) const
    {
        auto clone = std::make_unique<spPS2Keyboard>();
        manager.RegisterCloneForAnalysis(*this, *clone);
        // Original primary copy slot is the shared physical named-object copy,
        // not an input-map or device-state copy. The constructed +44 stays zero.
        if (!vfunc_14(*clone, manager)) return nullptr;
        return clone;
    }
    bool spPS2Keyboard::InitializeForAnalysis() noexcept
    { SetField44ForAnalysis(1); return true; }
    spPS2Keyboard::QueriesForAnalysis spPS2Keyboard::GetQueriesForAnalysis() const
    {
        QueriesForAnalysis queries;
        queries.slot1 = [this](auto p) { return PhysicalSlot1ForAnalysis(p); };
        queries.slot2 = [this](auto p) { return PhysicalSlot2ForAnalysis(p); };
        queries.slot3 = [this](auto p) { return PhysicalSlot3ForAnalysis(p); };
        queries.slot4 = [this](auto p) { return PhysicalSlot4ForAnalysis(p); };
        queries.slot5 = [this](auto p, auto a, auto b, auto c) { PhysicalSlot5ForAnalysis(p, a, b, c); };
        queries.slot6 = [this](auto p) { return PhysicalSlot6ForAnalysis(p); };
        return queries;
    }
}
