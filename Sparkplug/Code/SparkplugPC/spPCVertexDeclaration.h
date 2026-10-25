#pragma once
// Inferred path; original spPCVertexDeclaration ID66353288 is PC-only.
#include "../SparkplugDX/spDXVertexDeclaration.h"
#include "spPCVertexDeclarationElements.h"
#include "spPCVertexDeclarationDevice.h"

namespace sparkplug::reconstruction
{
    class spPCVertexDeclaration final : public spDXVertexDeclaration
    {
    public:
        static constexpr spClassID ClassID=0x66353288;
        spPCVertexDeclaration() noexcept = default;
        spPCVertexDeclaration(const spPCVertexDeclaration&)=delete;
        spPCVertexDeclaration& operator=(const spPCVertexDeclaration&)=delete;
        ~spPCVertexDeclaration() override;
        [[nodiscard]] static const spRTTIRecord& StaticRTTI() noexcept;
        [[nodiscard]] const spRTTIRecord& vfunc_18() const noexcept override;
        [[nodiscard]] std::unique_ptr<spBaseObject> vfunc_10(spCloneManager&) const override;
        // CPU-only facade; prior device initialization is refused before
        // mutation, including known-null/released device state.
        [[nodiscard]] bool InitializeForAnalysis(std::uint32_t componentFlags) override;
        // PC4C9A00->13D6F00. Used elements include terminator; native excess
        // allocation remains uninitialized and is not exposed as valid elements.
        [[nodiscard]] static bool BuildElementsForAnalysis(std::uint32_t componentFlags,
            std::vector<spPCVertexElementForAnalysis>& elements,std::uint32_t& nativeAllocationBytes);
        [[nodiscard]] const auto& GetElementsForAnalysis() const noexcept { return elements_; }
        [[nodiscard]] std::uint32_t GetNativeAllocationBytesForAnalysis() const noexcept { return allocationBytes_; }
        [[nodiscard]] bool IsInitializedForAnalysis() const noexcept { return !elements_.empty(); }
        // Concrete original CPU builder plus COM endpoint. HRESULT is ignored;
        // temporary output reuses the flags stack slot and is copied to18.
        // An unwritten nonzero seed is retained as an unsafe raw value with
        // incomplete host status; Bind/Release refuse that value. This does
        // not claim original unsafe teardown succeeded. Reinit leaks an overwritten
        // old handle exactly as the original; no hidden rollback/Release.
        // The release callback/context must outlive this object and cover all
        // returned handles. Changing that owner while a handle is live is an
        // explicit host refusal before mutation, not original validation.
        [[nodiscard]] spPCVertexDeclarationResultForAnalysis InitializeDeviceForAnalysis(
            std::uint32_t componentFlags,std::uintptr_t device,
            const spPCVertexDeclarationDeviceForAnalysis&);
        [[nodiscard]] spPCVertexDeclarationResultForAnalysis BindDeviceForAnalysis(
            std::uintptr_t device,const spPCVertexDeclarationDeviceForAnalysis&,
            std::uintptr_t ignoredOriginalArgument=0) const noexcept;
        [[nodiscard]] bool ReleaseDeviceForAnalysis() noexcept;
        [[nodiscard]] std::uintptr_t GetDeviceHandleForAnalysis() const noexcept{return deviceHandle_;}
        [[nodiscard]] bool HasInitializedDeviceHandleForAnalysis() const noexcept{return deviceHandleInitialized_;}
        [[nodiscard]] bool HasDeviceInitializationForAnalysis() const noexcept{return deviceInitializationAttempted_;}
    private:
        std::vector<spPCVertexElementForAnalysis> elements_;
        std::uint32_t allocationBytes_=0;
        std::uintptr_t deviceHandle_=0; // original18
        bool deviceHandleInitialized_=true; // host ownership eligibility, not native storage
        bool deviceInitializationAttempted_=false; // separates the extra CPU-only facade
        void* releaseContext_=nullptr;
        std::uint32_t (*releaseDevice_)(void*,std::uintptr_t) noexcept=nullptr;
    };
}
