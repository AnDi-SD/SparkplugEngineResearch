#include "wxCharacter.h"
#include <cstring>
#include <initializer_list>
#include <stdexcept>

namespace winx::reconstruction
{
    using namespace sparkplug::reconstruction;
    namespace
    {
        wxCharacterHost* factoryHost = nullptr;
        std::unique_ptr<spBaseObject> CreateCharacter()
        {
            if (!factoryHost) throw std::logic_error("wxCharacter requires a factory host");
            return std::make_unique<wxCharacter>(*factoryHost);
        }
        const spRTTIRecord spEntityRecord{0x22875AA1, spNamedObject::ClassID,
            "spEntity", &spNamedObject::StaticRTTI(), nullptr, nullptr};
        const spRTTIRecord wxEntityRecord{0x796A1869, 0x22875AA1,
            "wxEntity", &spEntityRecord, nullptr, nullptr};
        const spRTTIRecord record{wxCharacter::ClassID, 0x796A1869,
            "wxCharacter", &wxEntityRecord, &CreateCharacter, nullptr};
        const bool registered = spRTTIManager::Instance().RegisterDeferredForAnalysis(record);

        std::uint32_t ReadWord(const wxCharacter::OwnBytesForAnalysis& bytes,
            const std::size_t pcOffset) noexcept
        {
            std::uint32_t value;
            std::memcpy(&value, bytes.data() + pcOffset - 0x124, sizeof(value));
            return value;
        }
    }

    wxCharacter::wxCharacter(wxCharacterHost& host) : host_(host)
    {
        host_.ConstructEntityForAnalysis(*this);
        // Both constructors zero +124..+144/+130..+150 and set the link
        // selector at +148/+154 to one. Other native bytes are not inferred.
        ownBytes_[0x148 - 0x124] = 1;
        host_.RegisterCharacterForAnalysis(*this, 0x19);
        host_.RegisterCharacterForAnalysis(*this, 0x25);
    }

    wxCharacter::~wxCharacter()
    {
        if (ReadWord(ownBytes_, 0x148) == 1)
        {
            const auto reference = ReadWord(ownBytes_, 0x130);
            if (reference)
            {
                host_.ReleaseReferenceForAnalysis(*this, 0x130, reference);
                std::memset(ownBytes_.data() + 0x130 - 0x124, 0, 4);
            }
        }
        const auto reference = ReadWord(ownBytes_, 0x154);
        if (reference)
        {
            host_.ReleaseReferenceForAnalysis(*this, 0x154, reference);
            std::memset(ownBytes_.data() + 0x154 - 0x124, 0, 4);
        }
        host_.UnregisterCharacterForAnalysis(*this, 0x19);
        host_.UnregisterCharacterForAnalysis(*this, 0x25);
        host_.DetachCharacterForAnalysis(*this);
        host_.DestroyEntityForAnalysis(*this);
    }
    void wxCharacter::SetFactoryHostForAnalysis(wxCharacterHost* host) noexcept { factoryHost = host; }
    const spRTTIRecord& wxCharacter::StaticRTTI() noexcept { (void)registered; return record; }
    const spRTTIRecord& wxCharacter::vfunc_18() const noexcept { return record; }

    std::unique_ptr<spBaseObject> wxCharacter::vfunc_10(spCloneManager& manager) const
    {
        auto clone = std::make_unique<wxCharacter>(host_);
        manager.RegisterCloneForAnalysis(*this, *clone);
        if (!vfunc_14(*clone, manager)) return nullptr;
        return clone;
    }

    bool wxCharacter::vfunc_14(spBaseObject& destination, spCloneManager& manager) const
    {
        auto* target = dynamic_cast<wxCharacter*>(&destination);
        if (!target) return false; // Native caller supplies a valid destination.
        if (!host_.CopyEntityForAnalysis(*this, *target, manager)) return false;
        // Exact own-field order in PC 004F40A0 and PS2 002A95A0.
        for (const std::size_t offset : {0x124u, 0x12Cu, 0x130u, 0x134u,
             0x128u, 0x138u, 0x13Cu, 0x148u, 0x14Cu, 0x154u})
            std::memcpy(target->ownBytes_.data() + offset - 0x124,
                ownBytes_.data() + offset - 0x124, 4);
        target->ownBytes_[0x150 - 0x124] = ownBytes_[0x150 - 0x124];
        return true;
    }

    void wxCharacter::vfunc_28_AssignReferenceForAnalysis(void* const reference)
    {
        host_.AssignEntityReferenceForAnalysis(*this, reference);
        if (!ReadWord(ownBytes_, 0x148))
            host_.ClearExternalFlagForAnalysis(*this, 0x10);
    }

    bool wxCharacter::vfunc_2C_FlagsAllowForAnalysis(const std::uint32_t flags) const noexcept
    {
        return (flags & (ReadWord(ownBytes_, 0x148) ? 0x1Au : 0x08u)) == 0;
    }

    void wxCharacter::Handle2749ForAnalysis(const std::uint32_t kind,
        wxCharacter*& result) noexcept
    {
        if (ReadWord(ownBytes_, 0x14C) == kind && ownBytes_[0x150 - 0x124])
            result = this;
    }
}
