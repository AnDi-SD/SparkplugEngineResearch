#pragma once
// Inferred path; original spPCVertexDeclaration ID66353288 is PC-only.
#include "../SparkplugDX/spDXVertexDeclaration.h"
#include <vector>

namespace sparkplug::reconstruction
{
    struct spPCVertexElementForAnalysis final
    {
        std::uint16_t stream=0,offset=0;
        std::uint8_t type=0,method=0,usage=0,usageIndex=0;
    };
    static_assert(sizeof(spPCVertexElementForAnalysis)==8);

    class spPCVertexDeclaration final : public spDXVertexDeclaration
    {
    public:
        static constexpr spClassID ClassID=0x66353288;
        spPCVertexDeclaration() noexcept = default;
        ~spPCVertexDeclaration() override = default;
        [[nodiscard]] static const spRTTIRecord& StaticRTTI() noexcept;
        [[nodiscard]] const spRTTIRecord& vfunc_18() const noexcept override;
        [[nodiscard]] std::unique_ptr<spBaseObject> vfunc_10(spCloneManager&) const override;
        [[nodiscard]] bool InitializeForAnalysis(std::uint32_t componentFlags) override;
        // PC4C9A00->13D6F00. Used elements include terminator; native excess
        // allocation remains uninitialized and is not exposed as valid elements.
        [[nodiscard]] static bool BuildElementsForAnalysis(std::uint32_t componentFlags,
            std::vector<spPCVertexElementForAnalysis>& elements,std::uint32_t& nativeAllocationBytes);
        [[nodiscard]] const auto& GetElementsForAnalysis() const noexcept { return elements_; }
        [[nodiscard]] std::uint32_t GetNativeAllocationBytesForAnalysis() const noexcept { return allocationBytes_; }
        [[nodiscard]] bool IsInitializedForAnalysis() const noexcept { return !elements_.empty(); }
        // This source reconstructs CPU declaration contents, not a COM device.
        // Bind/COM reset/reinitialize/renderer ownership are separately open.
    private:
        std::vector<spPCVertexElementForAnalysis> elements_;
        std::uint32_t allocationBytes_=0;
    };
}
