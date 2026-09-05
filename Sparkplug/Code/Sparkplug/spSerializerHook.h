#pragma once

// Inferred reconstruction path. The exact class names are present in the
// shipped PC/PS2 executables, but no original header or translation-unit path
// has been recovered for this cluster. Slot-based names therefore remain
// analytical and do not claim the lost source API.

#include "../SparkBase/spBaseObject.h"

#include <cstdint>
#include <vector>

namespace sparkplug::reconstruction
{
    class spStream;
    class spResourceFATHelperForAnalysis;
    struct spResourceFATEntryForAnalysis;

    class spSerializerHook : public spBaseObject
    {
    public:
        static constexpr spClassID ClassID = 0x18092F8D;

        spSerializerHook() noexcept = default;
        ~spSerializerHook() override;

        spSerializerHook(const spSerializerHook&) = delete;
        spSerializerHook& operator=(const spSerializerHook&) = delete;

        [[nodiscard]] static const spRTTIRecord& StaticRTTI() noexcept;

        // Native PS2 clone slot returns null for this abstract base.
        [[nodiscard]] std::unique_ptr<spBaseObject> vfunc_10(
            spCloneManager& manager) const override;
        [[nodiscard]] const spRTTIRecord& vfunc_18() const noexcept override;

        // Exact PS2 vtable slot +0x24. The caller passes the manager-owned FAT
        // helper and source stream. The helper's exact C++ type and the method
        // name are not present, so its reconstructed type is explicitly marked
        // ForAnalysis rather than presented as a recovered declaration.
        virtual void vfunc_24(
            spResourceFATHelperForAnalysis* fatHelper,
            spStream& source) = 0;
    };

    class spPS2SerializerHook final : public spSerializerHook
    {
    public:
        static constexpr spClassID ClassID = 0x1C0E0F30;

        spPS2SerializerHook() noexcept = default;
        ~spPS2SerializerHook() override;

        spPS2SerializerHook(const spPS2SerializerHook&) = delete;
        spPS2SerializerHook& operator=(const spPS2SerializerHook&) = delete;

        [[nodiscard]] static const spRTTIRecord& StaticRTTI() noexcept;

        [[nodiscard]] std::unique_ptr<spBaseObject> vfunc_10(
            spCloneManager& manager) const override;
        [[nodiscard]] const spRTTIRecord& vfunc_18() const noexcept override;
        void vfunc_24(
            spResourceFATHelperForAnalysis* fatHelper,
            spStream& source) override;
    };

    struct spDXMeshNativeInfoForAnalysis final
    {
        std::uint32_t objectClassID = 0;
        std::uint32_t objectMarker = 0;
        std::uint32_t fvfCode = 0;
        std::uint32_t vertexCount = 0;
        std::uint32_t vertexDataSize = 0;
        std::uint32_t indexDataSize = 0;
        bool indicesAre32Bit = false;
        bool foundNativeField = false;
    };

    struct spDXMeshBatchForAnalysis final
    {
        std::uint32_t fvfCode = 0;
        std::uint32_t vertexCount = 0;
        std::uint32_t vertexDataSize = 0;
        std::uint32_t indexDataSize = 0;
        std::vector<std::uint32_t> resourceIDs;
    };

    // Implemented by the original PC translation unit
    // Z:\Sparkplug\Code\SparkplugDX\spDXMesh.cpp. The GPU container is now
    // reconstructed separately; full serializer dispatch remains outside this
    // portable hook slice.
    class spDXSerializerHook final : public spSerializerHook
    {
    public:
        static constexpr spClassID ClassID = 0x0D832A30;
        static constexpr std::uint32_t MaximumCombinedVertexCount = 0x4E20;

        spDXSerializerHook() noexcept = default;
        ~spDXSerializerHook() override;

        spDXSerializerHook(const spDXSerializerHook&) = delete;
        spDXSerializerHook& operator=(const spDXSerializerHook&) = delete;

        [[nodiscard]] static const spRTTIRecord& StaticRTTI() noexcept;
        [[nodiscard]] std::unique_ptr<spBaseObject> vfunc_10(
            spCloneManager& manager) const override;
        [[nodiscard]] const spRTTIRecord& vfunc_18() const noexcept override;
        void vfunc_24(
            spResourceFATHelperForAnalysis* fatHelper,
            spStream& source) override;

        // PC 0x004AA4E0: seek to one FAT object, ignore the SBOO marker just
        // like native code, walk all data blocks and read field 1 as the five
        // DX mesh-buffer values.
        [[nodiscard]] static bool ReadDXMeshDataInfoForAnalysis(
            spStream& source,
            const spResourceFATEntryForAnalysis& entry,
            spDXMeshNativeInfoForAnalysis& info) noexcept;

        // Confirmed first pass of PC 0x004AA870. Unresolved spMeshData entries
        // are greedily grouped by FVF while the running vertex total remains
        // strictly below 0x4E20. The returned plan deliberately stops before
        // constructing/dispatching a batch through spDXMeshCombiner.
        [[nodiscard]] static bool BuildBatchPlanForAnalysis(
            spResourceFATHelperForAnalysis& fatHelper,
            spStream& source,
            std::vector<spDXMeshBatchForAnalysis>& plan) noexcept;

        [[nodiscard]] const std::vector<spDXMeshBatchForAnalysis>&
            GetLastBatchPlanForAnalysis() const noexcept;
        [[nodiscard]] static constexpr bool
            HasCompleteNativeMaterializationForAnalysis() noexcept
        {
            return false;
        }

    private:
        std::vector<spDXMeshBatchForAnalysis> lastBatchPlan_;
    };
}
