#pragma once

// Exact original PC translation-unit path recovered from diagnostics:
//   Z:\Sparkplug\Code\Sparkplug\spDataBlockSerializer.cpp
// The original header and the data-block header type name do not survive.
// Names ending in ForAnalysis are therefore portable seams around the proven
// wire grammar rather than claims about the lost public declarations.

#include "../SparkBase/spStream.h"

#include <cstdint>
#include <vector>

namespace sparkplug::reconstruction
{
    struct spDataBlockHeaderForAnalysis final
    {
        static constexpr std::uint32_t InvalidFieldID = 0xFFFFFFFFU;

        std::uint32_t fieldID = InvalidFieldID;
        std::uint32_t payloadSize = 0;
        std::uint32_t headerStreamPosition = 0;
        std::uint32_t dataStreamPosition = 0;

        [[nodiscard]] bool IsTerminator() const noexcept
        {
            return fieldID == InvalidFieldID;
        }
    };

    static_assert(sizeof(spDataBlockHeaderForAnalysis) == 0x10);

    class spDataBlockSerializer final
    {
    public:
        enum class SizeCode : std::uint8_t
        {
            Empty = 0,
            Fixed1 = 1,
            Fixed2 = 2,
            Fixed4 = 3,
            Fixed8 = 4,
            UInt8 = 5,
            UInt16 = 6,
            UInt32 = 7,
        };

        static constexpr std::uint32_t InlineFieldIDLimit = 0x1FU;
        static constexpr std::uint32_t ExtendedFieldIDLimit = 0xFFU;

        spDataBlockSerializer() noexcept = default;

        // PC 0x004728F0 / PS2 0x0017E890. A size-code zero byte is the
        // section terminator: native code replaces its decoded field ID with
        // 0xFFFFFFFF and gives it an empty payload.
        [[nodiscard]] const spDataBlockHeaderForAnalysis*
            ReadHeaderForAnalysis(spStream& source) noexcept;

        // PC 0x00472AC0 / PS2 0x0017E830 seek to data-position + size rather
        // than consuming a payload through ReadData.
        [[nodiscard]] static bool SkipDataForAnalysis(
            spStream& source,
            const spDataBlockHeaderForAnalysis& header) noexcept;

        // A safe direct-field writer built from the native WriteHeader and
        // WriteData sequence. It uses the same smallest representable size
        // code selected by PC 0x00472730 / PS2 0x0017E740. Explicit host
        // corrections: native PC WriteHeader omits size-zero fields entirely
        // and treats ID31 as inline despite the reader's escape rule. This
        // direct helper emits valid wire headers for both, NOT identical bugs.
        [[nodiscard]] bool WriteFieldForAnalysis(
            spStream& destination,
            std::uint32_t fieldID,
            const void* payload,
            std::uint32_t payloadSize) const noexcept;

        // PC 0x00472B00 / PS2 0x0017E7C0 emit one zero byte. This is distinct
        // from a legitimate zero-length field, whose native size code is 5.
        [[nodiscard]] static bool WriteTerminatorForAnalysis(
            spStream& destination) noexcept;

        [[nodiscard]] static SizeCode SelectSizeCodeForAnalysis(
            std::uint32_t payloadSize) noexcept;
        // Existing common header encoder, exposed for bounded metadata edits.
        // Its historical direct-host corrections for ID31/size0 are NOT new
        // native evidence. The tools bridge rejects those nonstandard forms
        // and uses the original terminator operation explicitly when requested.
        [[nodiscard]] static bool WriteHeaderWithCodeForAnalysis(
            spStream& destination, std::uint32_t fieldID,
            std::uint32_t payloadSize, SizeCode sizeCode) noexcept;

        // PC472710 stores the object and stream, without writing any bytes.
        // Writer API names are diagnostic-backed; ForAnalysis marks host
        // bounds and the portable container, not a native C++ declaration.
        [[nodiscard]] bool BeginObjectForAnalysis(
            spStream& destination, const spBaseObject* object = nullptr) noexcept;
        [[nodiscard]] bool WriteBeginForAnalysis(
            std::uint32_t fieldID, SizeCode reservedSizeCode = SizeCode::UInt32) noexcept;
        // PC472E20 uses the top header, ignoring the supplied field ID.
        [[nodiscard]] bool WriteEndForAnalysis(std::uint32_t ignoredFieldID) noexcept;
        [[nodiscard]] bool FinalizeObjectForAnalysis() noexcept;
        [[nodiscard]] std::size_t GetOpenFieldCountForAnalysis() const noexcept
        {
            return writerHeaders_.size();
        }
        // Native keeps ONE reserved-size code, not one per header. Mixed
        // nested widths corrupt outer headers. This portable boundary rejects
        // mixed nesting, fieldID31 and fixed-width reservation up front, and
        // rejects empty/oversized fields at End; no fake native support or
        // silent repair. Uniform UInt8/16/32 with nonempty payloads works.
        [[nodiscard]] const spDataBlockHeaderForAnalysis&
            GetCurrentHeaderForAnalysis() const noexcept;

    private:
        [[nodiscard]] static bool WriteHeaderForAnalysis(
            spStream& destination,
            std::uint32_t fieldID,
            std::uint32_t payloadSize) noexcept;

        spDataBlockHeaderForAnalysis currentHeader_{};
        spStream* writerStream_ = nullptr;
        const spBaseObject* writerObject_ = nullptr; // stored, never owned/dereferenced here
        SizeCode reservedSizeCode_ = SizeCode::Empty;
        std::vector<spDataBlockHeaderForAnalysis> writerHeaders_;
    };
}
