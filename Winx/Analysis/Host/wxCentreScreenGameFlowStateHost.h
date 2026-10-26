#pragma once
#include "wxGameFlowStateHost.h"
#include "Code/Sparkplug/spMaterial.h"
#include "Code/Sparkplug/spMaterialPassLayer.h"
#include "Code/Sparkplug/spStdLayer.h"

namespace winx::reconstruction
{
    // Typed foreign-class factories and external storage. Material allocations
    // live in the host arena, independently of a state's raw pointer. In
    // particular, replacing that pointer or destroying a state does not release
    // the arena allocation. Native16-bit refcounts are separate from C++ owners.
    class wxCentreScreenGameFlowStateHost : public wxGameFlowStateHost
    {
    public:
        // PC4A9460 creates spDXMaterial. PS21F2A90 is an allocating
        // spPS2Material constructor with static color inputs, not its registered
        // factory1F2FC0. Returned objects must be real reconstructed materials.
        virtual sparkplug::reconstruction::spMaterial* CreateCentreMaterialForAnalysis() = 0;
        virtual std::uint16_t& BorrowCentreMaterialRefCount8ForAnalysis(
            sparkplug::reconstruction::spMaterial&) = 0;
        virtual void DestroyCentreMaterialForAnalysis(
            sparkplug::reconstruction::spMaterial&,std::uint32_t deletingFlag) = 0;
        virtual std::shared_ptr<sparkplug::reconstruction::spMaterialPassLayer>
            CreateCentrePassForAnalysis() = 0;
        virtual std::unique_ptr<sparkplug::reconstruction::spStdLayer>
            CreateCentreStdLayerForAnalysis() = 0;
        // These two owner resolves are distinct native global loads. The words
        // have no established semantic name or floating-point interpretation.
        virtual void* ResolveCentreAppOwnerForAnalysis() = 0;
        virtual std::uint32_t ReadCentreAppWord64ForAnalysis(void* owner) = 0;
        virtual std::uint32_t ReadCentreAppWord68ForAnalysis(void* owner) = 0;
        virtual void* ResolveCentreWindowForAnalysis() = 0;
        virtual std::uint32_t& BorrowCentreWindowWord1294ForAnalysis(void* window) = 0;
    };
}
