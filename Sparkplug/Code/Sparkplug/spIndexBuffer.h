#pragma once

// Inferred declaration path.  The class and nested enum type name survive in
// both executables/call-site text, but no original header or translation-unit
// path has been recovered.

#include "../SparkBase/spBaseObject.h"

#include <cstddef>
#include <cstdint>
#include <memory>
#include <optional>
#include <vector>

namespace sparkplug::reconstruction
{
    class spStream;

    class spIndexBuffer final : public spBaseObject
    {
    public:
        static constexpr spClassID ClassID = 0x77D5669F;

        // The original type spelling is preserved by a PC mesh assertion.
        // The enumerator spellings are not; analytical names retain the four
        // native numeric values without inventing topology terminology.
        enum class eIndexBufferType : std::uint32_t
        {
            Type1 = 1,
            Type2 = 2,
            Type3 = 3,
            Type4 = 4,
        };

        spIndexBuffer() noexcept = default;
        ~spIndexBuffer() override;

        spIndexBuffer(const spIndexBuffer&) = delete;
        spIndexBuffer& operator=(const spIndexBuffer&) = delete;

        [[nodiscard]] static const spRTTIRecord& StaticRTTI() noexcept;

        [[nodiscard]] std::unique_ptr<spBaseObject> vfunc_10(
            spCloneManager& manager) const override;
        [[nodiscard]] const spRTTIRecord& vfunc_18() const noexcept override;

        // Portable names for native nonvirtual routines.  The public native
        // initializer accepts primitive count and expands it into index count.
        [[nodiscard]] bool InitializeForAnalysis(
            std::uint32_t primitiveCount,
            eIndexBufferType type,
            std::uint32_t formatFlags = 0);
        void ReleaseForAnalysis() noexcept;

        // Explicit host wire-byte budget includes the12-byte header. Rejects
        // oversized counts before allocation; not a recovered native guard.
        [[nodiscard]] bool ReadForAnalysis(spStream& stream,
            std::uint32_t maximumSerializedBytes = 0xffffffffu);
        [[nodiscard]] bool WriteForAnalysis(spStream& stream) const;

        // Native PS2 0x00159780 performs this independent deep-copy operation;
        // it is deliberately distinct from RTTI Clone(), which is blank.
        [[nodiscard]] std::unique_ptr<spIndexBuffer>
            CopyBufferForAnalysis() const;

        [[nodiscard]] bool IsInitializedForAnalysis() const noexcept;
        [[nodiscard]] eIndexBufferType GetTypeForAnalysis() const noexcept;
        [[nodiscard]] std::uint32_t GetPrimitiveCountForAnalysis() const noexcept;
        [[nodiscard]] std::uint32_t GetIndexCountForAnalysis() const noexcept;
        [[nodiscard]] std::uint32_t GetFormatFlagsForAnalysis() const noexcept;
        [[nodiscard]] std::size_t GetIndexElementSizeForAnalysis() const noexcept;
        [[nodiscard]] bool SetIndexForAnalysis(
            std::uint32_t position,
            std::uint32_t value) noexcept;
        [[nodiscard]] std::optional<std::uint32_t> GetIndexForAnalysis(
            std::uint32_t position) const noexcept;

    private:
        [[nodiscard]] static std::optional<std::uint32_t> IndexCountFor(
            eIndexBufferType type,
            std::uint32_t primitiveCount) noexcept;

        bool initialized_ = false;
        eIndexBufferType type_ = eIndexBufferType::Type2;
        std::uint32_t primitiveCount_ = 0;
        std::uint32_t indexCount_ = 0;
        std::uint32_t formatFlags_ = 0;
        std::vector<std::uint32_t> indices_;
    };
}
