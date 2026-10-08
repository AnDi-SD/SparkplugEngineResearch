#pragma once

// Exact original path proven by the PC diagnostic strings:
// Z:\Sparkplug\Code\Sparkplug\spTemplateSerializer.cpp

#include "../SparkBase/spBaseObject.h"

#include <array>
#include <cstdint>
#include <memory>
#include <optional>

namespace sparkplug::reconstruction
{
    class spStream;
    class spTemplate;

    class spTemplateSerializer final : public spBaseObject
    {
    public:
        static constexpr spClassID ClassID = 0x41577707;
        static constexpr std::uint32_t BinaryHeaderMagic = 0xDAB33F00;
        static constexpr std::uint32_t BinaryHeaderSize = 0x4C;

        spTemplateSerializer() noexcept;
        ~spTemplateSerializer() override;

        spTemplateSerializer(const spTemplateSerializer&) = delete;
        spTemplateSerializer& operator=(const spTemplateSerializer&) = delete;

        [[nodiscard]] static const spRTTIRecord& StaticRTTI() noexcept;

        [[nodiscard]] std::unique_ptr<spBaseObject> vfunc_10(
            spCloneManager& manager) const override;
        bool vfunc_14(spBaseObject& destination, spCloneManager& manager) const override;
        [[nodiscard]] const spRTTIRecord& vfunc_18() const noexcept override;

        // Native serializer entry paths install these non-owning pointers at
        // +0x10/+0x14. Exact public method names have not survived.
        void BindForAnalysis(spTemplate* target, spStream* input) noexcept;
        [[nodiscard]] spTemplate* GetTargetForAnalysis() const noexcept;
        [[nodiscard]] spStream* GetInputForAnalysis() const noexcept;
        [[nodiscard]] std::int32_t GetParentIDForAnalysis() const noexcept;
        [[nodiscard]] bool HasOutputForAnalysis() const noexcept;

        // PC005FC310 / inline PS200157EC8..00157FEC: one raw 0x4C-byte read
        // into a zeroed header, magic check, template +20 then inherited name.
        // The full uint32 result is the descriptor count, not a boolean. A
        // valid zero count still changes word/name; the outer file reader then
        // reports failure. ReadData's boolean is used without a byte-count
        // check, including successful partial reads. No descriptor parser is
        // implied by this bounded header operation.
        // Null host bindings or a name without a terminator inside its 64-byte
        // field return no result; those unsafe native domains are not emulated.
        [[nodiscard]] std::optional<std::uint32_t> ReadBinaryHeaderForAnalysis();

        // HOST forwarding for the original invalid-magic diagnostic. Its
        // callback/context are not native serializer fields. No diagnostic is
        // forwarded when the host leaves this boundary unbound.
        using HeaderDiagnosticForAnalysis = void (*)(void*);
        void SetHeaderDiagnosticForAnalysis(HeaderDiagnosticForAnalysis callback,
            void* context) noexcept { headerDiagnostic_ = callback; diagnosticContext_ = context; }

    private:
        spTemplate* target_ = nullptr;
        spStream* input_ = nullptr;
        void* parsedDocument_ = nullptr;
        std::int32_t parentID_ = -1;
        std::array<std::uint8_t, 0x40> parentName_{};
        std::shared_ptr<spBaseObject> output_;
        bool outputReady_ = false;
        std::uint32_t field1EC_ = 0;
        bool failed_ = false;
        HeaderDiagnosticForAnalysis headerDiagnostic_ = nullptr;
        void* diagnosticContext_ = nullptr;
    };
}
