#include "wxSpiderAttackAIAction.h"
#include "wxAICommandRecordForAnalysis.h"
#include "Analysis/PC/wxSpiderAttackAIActionAbi.h"
#include <stdexcept>

namespace winx::reconstruction
{
    using namespace sparkplug::reconstruction;
    namespace
    {
        std::unique_ptr<spBaseObject> CreateCapturedStateGate()
        { throw std::logic_error("wxSpiderAttackAIAction requires supplied state, owner and raw storage"); }
        const spRTTIRecord record{wxSpiderAttackAIAction::ClassID, wxAIAction::ClassID,
            "wxSpiderAttackAIAction", &wxAIAction::StaticRTTI(), &CreateCapturedStateGate, nullptr};
        const bool registered = spRTTIManager::Instance().RegisterDeferredForAnalysis(record);
    }
    wxSpiderAttackAIAction::wxSpiderAttackAIAction(const CapturedState& state, void* owner,
        wxAIAttackStateStorageHost* storage, ProfileForAnalysis profile) noexcept
        : state_(state), storage_(storage), profile_(profile)
    {
        SetOwnerForAnalysis(owner);
        SetDurationMillisecondsForAnalysis(ReadWordForAnalysis(0x370));
        SetClearableWordForAnalysis(ReadWordForAnalysis(0x374));
    }
    const spRTTIRecord& wxSpiderAttackAIAction::StaticRTTI() noexcept { (void)registered; return record; }
    const spRTTIRecord& wxSpiderAttackAIAction::vfunc_18() const noexcept { return record; }
    std::unique_ptr<spBaseObject> wxSpiderAttackAIAction::vfunc_10(spCloneManager&) const
    { throw std::logic_error("Captured state-gate Clone remains unqualified"); }
    bool wxSpiderAttackAIAction::vfunc_14(spBaseObject&, spCloneManager&) const { return false; }
    std::uint32_t wxSpiderAttackAIAction::ReadWordForAnalysis(const std::size_t offset) const noexcept
    {
        std::uint32_t value = 0;
        for (unsigned byte = 0; byte != 4; ++byte) value |= std::uint32_t{state_[offset + byte]} << (byte * 8);
        return value;
    }
    void wxSpiderAttackAIAction::PointerReadForAnalysis(const void* object, std::uint32_t offset, const void* pointer) noexcept
    { accesses_[accessCount_++] = {AccessForAnalysis::Kind::ReadPointer, object, offset, 4, 0, pointer}; }
    void wxSpiderAttackAIAction::ValueReadForAnalysis(const void* object, std::uint32_t offset, std::uint8_t bytes, std::uint32_t value) noexcept
    { accesses_[accessCount_++] = {AccessForAnalysis::Kind::ReadValue, object, offset, bytes, value, nullptr}; }
    void wxSpiderAttackAIAction::ValueWriteForAnalysis(const void* object, std::uint32_t offset, std::uint8_t bytes, std::uint32_t value) noexcept
    { accesses_[accessCount_++] = {AccessForAnalysis::Kind::WriteValue, object, offset, bytes, value, nullptr}; }
    void wxSpiderAttackAIAction::WriteWordForAnalysis(const std::size_t offset, const std::uint32_t value) noexcept
    {
        for (unsigned byte = 0; byte != 4; ++byte) state_[offset + byte] = static_cast<std::uint8_t>(value >> (byte * 8));
        ValueWriteForAnalysis(this, static_cast<std::uint32_t>(offset), 4, value);
    }
    void wxSpiderAttackAIAction::WriteByteForAnalysis(const std::size_t offset, const std::uint8_t value) noexcept
    { state_[offset] = value; ValueWriteForAnalysis(this, static_cast<std::uint32_t>(offset), 1, value); }
    wxSpiderAttackAIAction::ResultForAnalysis wxSpiderAttackAIAction::StateGateForAnalysis() noexcept
    {
        accessCount_ = 0;
        if (profile_ != ProfileForAnalysis::PcCapturedStableDisjointStorage
            || GetCurrentActionForAnalysis() != nullptr || GetOwnedActionCountForAnalysis() != 0)
            return lastResult_ = ResultForAnalysis::UnqualifiedProfile;
        if (ReadWordForAnalysis(0) != evidence::pc::wxSpiderAttackAIActionPrimary)
            return lastResult_ = ResultForAnalysis::CapturedPrimaryNotAdmitted;
        const auto flag = state_[0x3BC];
        ValueReadForAnalysis(this, 0x3BC, 1, flag);
        auto* const owner = GetOwnerForAnalysis();
        PointerReadForAnalysis(this, 0x20, owner);
        if (!owner) return lastResult_ = ResultForAnalysis::MissingOwner;
        if (!storage_) return lastResult_ = ResultForAnalysis::MissingStorage;
        const auto actor = storage_->ReadActor144ForAnalysis(owner);
        if (!actor) return lastResult_ = ResultForAnalysis::MissingActor;
        PointerReadForAnalysis(owner, 0x144, *actor);
        if (!*actor) return lastResult_ = ResultForAnalysis::MissingActor;
        const auto state = storage_->ReadState124ForAnalysis(*actor);
        if (!state) return lastResult_ = ResultForAnalysis::MissingState;
        PointerReadForAnalysis(*actor, 0x124, *state);
        if (!*state) return lastResult_ = ResultForAnalysis::MissingState;
        const auto word = storage_->ReadStateWord130ForAnalysis(*state);
        if (!word) return lastResult_ = ResultForAnalysis::MissingStateValue;
        ValueReadForAnalysis(*state, 0x130, 4, *word);
        if (flag == 0)
        {
            const auto command = storage_->ReadCommand130ForAnalysis(*actor);
            if (!command) return lastResult_ = ResultForAnalysis::MissingCommand;
            PointerReadForAnalysis(*actor, 0x130, *command);
            if (!*command) return lastResult_ = ResultForAnalysis::MissingCommand;
            const std::uint8_t value = *word == 3 ? 0 : 1;
            if ((*command)->StoreByte20ForAnalysis(value) != wxAICommandRecordForAnalysis::ResultForAnalysis::Completed)
                return lastResult_ = ResultForAnalysis::CommandNotAdmitted;
            ValueWriteForAnalysis(*command, 0x20, 1, value);
            if (*word == 3) WriteByteForAnalysis(0x3BC, 1);
        }
        else if (*word != 3)
        {
            WriteByteForAnalysis(0x3BC, 0);
            WriteWordForAnalysis(0x3AC, 0);
            WriteWordForAnalysis(0x3B4, 0);
        }
        return lastResult_ = ResultForAnalysis::Completed;
    }
    void wxSpiderAttackAIAction::vfunc_0C(const void*) noexcept { lastResult_ = ResultForAnalysis::UnqualifiedHook; }
    bool wxSpiderAttackAIAction::RefuseHookForAnalysis() noexcept { lastResult_ = ResultForAnalysis::UnqualifiedHook; return false; }
    bool wxSpiderAttackAIAction::vfunc_24() noexcept { return RefuseHookForAnalysis(); }
    bool wxSpiderAttackAIAction::vfunc_28() noexcept { return RefuseHookForAnalysis(); }
    bool wxSpiderAttackAIAction::vfunc_2C() noexcept { return RefuseHookForAnalysis(); }
    void wxSpiderAttackAIAction::vfunc_30() noexcept { (void)RefuseHookForAnalysis(); }
    void wxSpiderAttackAIAction::vfunc_34_ClearForAnalysis() noexcept { (void)RefuseHookForAnalysis(); }
    void wxSpiderAttackAIAction::vfunc_38() noexcept { (void)RefuseHookForAnalysis(); }
    bool wxSpiderAttackAIAction::vfunc_3C(const void*) noexcept { return RefuseHookForAnalysis(); }
    bool wxSpiderAttackAIAction::vfunc_40(const void*) noexcept { return RefuseHookForAnalysis(); }
}
