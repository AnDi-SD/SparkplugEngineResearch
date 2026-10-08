#include "spTemplateSerializer.h"
#include "spTemplate.h"
#include "../SparkBase/spStream.h"

#include <algorithm>
#include <utility>

namespace sparkplug::reconstruction
{
    namespace
    {
        std::unique_ptr<spBaseObject> CreateTemplateSerializer()
        {
            return std::make_unique<spTemplateSerializer>();
        }

        const spRTTIRecord TemplateSerializerRecord{
            spTemplateSerializer::ClassID,
            spBaseObject::ClassID,
            "spTemplateSerializer",
            &spBaseObject::StaticRTTI(),
            &CreateTemplateSerializer,
            nullptr,
        };

        const bool TemplateSerializerRegistered =
            spRTTIManager::Instance().RegisterDeferredForAnalysis(TemplateSerializerRecord);
    }

    spTemplateSerializer::spTemplateSerializer() noexcept = default;

    spTemplateSerializer::~spTemplateSerializer() = default;

    const spRTTIRecord& spTemplateSerializer::StaticRTTI() noexcept
    {
        (void)TemplateSerializerRegistered;
        return TemplateSerializerRecord;
    }

    std::unique_ptr<spBaseObject> spTemplateSerializer::vfunc_10(
        spCloneManager& manager) const
    {
        auto clone = std::make_unique<spTemplateSerializer>();
        manager.RegisterClone(*this, *clone);
        return vfunc_14(*clone, manager) ? std::move(clone) : nullptr;
    }

    bool spTemplateSerializer::vfunc_14(
        spBaseObject& destination,
        spCloneManager& manager) const
    {
        // Both native vtables reuse the root no-payload copy implementation.
        return spBaseObject::vfunc_14(destination, manager);
    }

    const spRTTIRecord& spTemplateSerializer::vfunc_18() const noexcept
    {
        return TemplateSerializerRecord;
    }

    void spTemplateSerializer::BindForAnalysis(
        spTemplate* const target,
        spStream* const input) noexcept
    {
        target_ = target;
        input_ = input;
    }

    spTemplate* spTemplateSerializer::GetTargetForAnalysis() const noexcept
    {
        return target_;
    }

    spStream* spTemplateSerializer::GetInputForAnalysis() const noexcept
    {
        return input_;
    }

    std::int32_t spTemplateSerializer::GetParentIDForAnalysis() const noexcept
    {
        return parentID_;
    }

    bool spTemplateSerializer::HasOutputForAnalysis() const noexcept
    {
        return output_ != nullptr;
    }

    std::optional<std::uint32_t> spTemplateSerializer::ReadBinaryHeaderForAnalysis()
    {
        if (!input_) return std::nullopt; // HOST guard around native dereference.
        std::array<std::uint8_t, BinaryHeaderSize> header{};
        if (!input_->ReadData(header.data(), BinaryHeaderSize)) return 0;
        const auto word = [&header](std::size_t offset) noexcept
        {
            return static_cast<std::uint32_t>(header[offset])
                | (static_cast<std::uint32_t>(header[offset + 1]) << 8)
                | (static_cast<std::uint32_t>(header[offset + 2]) << 16)
                | (static_cast<std::uint32_t>(header[offset + 3]) << 24);
        };
        if (word(0) != BinaryHeaderMagic)
        {
            if (headerDiagnostic_) headerDiagnostic_(diagnosticContext_);
            return 0;
        }
        // Native reloads target +10 after ReadData: a foreign callback can
        // change the binding while the stream operation runs.
        if (!target_ || std::find(header.begin() + 4, header.begin() + 0x44, 0)
            == header.begin() + 0x44) return std::nullopt; // HOST domain guard.
        target_->SetField20ForAnalysis(word(0x48));
        (void)target_->SetName(reinterpret_cast<const char*>(header.data() + 4));
        return word(0x44);
    }
}
