#pragma once

// Inferred declaration path. The native class name and several member names
// survive in diagnostic strings, but no original header/TU path was found.

#include "../SparkBase/spBaseObject.h"

#include <array>
#include <cstddef>
#include <cstdint>
#include <memory>
#include <vector>

namespace sparkplug::reconstruction
{
    class spStream;

    class spVertexBuffer final : public spBaseObject
    {
    public:
        static constexpr spClassID ClassID = 0x3C846352;
        static constexpr std::size_t ComponentOffsetCount = 22;

        // Analytical component bits. Their numeric values and widths are
        // exact; source enumerator spellings are not present in the binaries.
        enum class Component : std::uint32_t
        {
            Field26 = 0x000001,
            Field28 = 0x000002,
            Field2A = 0x000004,
            Field2C = 0x000008,
            Field2E = 0x000010,
            Field30 = 0x000020,
            Field32 = 0x000040,
            Field34 = 0x000080,
            Field36 = 0x000100,
            Field38 = 0x000200,
            Field3A = 0x000400,
            Field3C = 0x000800,
            Field3E = 0x001000,
            Field40 = 0x002000,
            Field42 = 0x004000,
            Field44 = 0x008000,
            Field46 = 0x010000,
            Field48 = 0x020000,
            Field4A = 0x040000,
            Field4C = 0x080000,
            Field4E = 0x100000,
        };

        spVertexBuffer() noexcept = default;
        ~spVertexBuffer() override;

        spVertexBuffer(const spVertexBuffer&) = delete;
        spVertexBuffer& operator=(const spVertexBuffer&) = delete;

        [[nodiscard]] static const spRTTIRecord& StaticRTTI() noexcept;

        [[nodiscard]] std::unique_ptr<spBaseObject> vfunc_10(
            spCloneManager& manager) const override;
        [[nodiscard]] const spRTTIRecord& vfunc_18() const noexcept override;

        // Safe portable names for native nonvirtual routines. The first word
        // is the exact m_uComponentFlags bit mask; flags is exact m_uFlags.
        // Weight/UV groups stop at the first missing bit. Layout rebuilds
        // retain absent component offsets; only construction clears the table.
        [[nodiscard]] bool InitializeForAnalysis(
            std::uint32_t componentFlags,
            std::uint32_t vertexCount,
            std::uint32_t flags = 0);
        [[nodiscard]] bool InitializeFromDataForAnalysis(
            std::uint32_t componentFlags,
            std::uint32_t vertexCount,
            std::uint32_t flags,
            const std::vector<std::byte>& bytes);
        [[nodiscard]] bool InitializeRawForAnalysis(std::uint32_t byteCount);
        void ReleaseForAnalysis() noexcept;

        // Host byte budget includes12-byte header; preflight shares the real
        // component-layout code and cannot allocate from an unchecked count.
        [[nodiscard]] bool ReadForAnalysis(spStream& stream,
            std::uint32_t maximumSerializedBytes = 0xffffffffu);
        [[nodiscard]] bool WriteForAnalysis(spStream& stream) const;
        [[nodiscard]] std::unique_ptr<spVertexBuffer>
            CopyBufferForAnalysis() const;

        [[nodiscard]] bool IsInitializedForAnalysis() const noexcept;
        [[nodiscard]] std::uint32_t GetComponentFlagsForAnalysis() const noexcept;
        [[nodiscard]] std::uint32_t GetVertexCountForAnalysis() const noexcept;
        [[nodiscard]] std::uint32_t GetFlagsForAnalysis() const noexcept;
        [[nodiscard]] std::uint16_t GetVertexStrideForAnalysis() const noexcept;
        [[nodiscard]] std::uint16_t GetComponentCountForAnalysis() const noexcept;
        [[nodiscard]] std::uint32_t GetVertexSizeForAnalysis() const noexcept;
        // Offsets are in 32-bit words. An absent component can retain an old
        // offset after reinitialization; it must not be inferred from offset alone.
        [[nodiscard]] const std::array<std::uint16_t, ComponentOffsetCount>&
            GetComponentOffsetsForAnalysis() const noexcept;
        [[nodiscard]] const std::vector<std::byte>& GetDataForAnalysis() const noexcept;
        [[nodiscard]] bool SetDataForAnalysis(
            const std::vector<std::byte>& bytes);

    private:
        void RebuildComponentLayoutForAnalysis() noexcept;
        [[nodiscard]] bool AllocateForCurrentLayoutForAnalysis();

        std::uint16_t vertexStride_ = 0;
        std::uint32_t componentFlags_ = 0;
        std::uint16_t componentCount_ = 0;
        std::uint32_t vertexCount_ = 0;
        std::uint32_t vertexSize_ = 0;
        std::array<std::uint16_t, ComponentOffsetCount> componentOffsets_{};
        std::uint32_t flags_ = 0;
        std::vector<std::byte> data_;
        bool initialized_ = false;
    };
}
