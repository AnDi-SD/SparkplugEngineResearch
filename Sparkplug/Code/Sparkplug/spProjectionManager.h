#pragma once

// Inferred source/header placement and API names. PC own behavior only;
// registration on PS2 does not qualify its methods or platform dependencies.
#include "Code/SparkBase/spBaseObject.h"
#include "Analysis/Host/spProjectionManagerHost.h"
#include <functional>
#include <list>
#include <vector>

namespace sparkplug::reconstruction
{
    class spPCProjectionFX;
    class spProjectionFxHost;
    enum class spProjectionFxInitializationResultForAnalysis;
    // Standalone PC4243D0 prefix with an already-bound helper94. The owning
    // source declaration is unknown. Null helper requires the unqualified
    // native4C51B0 factory and explicitly refuses before that path.
    [[nodiscard]] bool InitializeProjectionPCForAnalysis(void* projection,spProjectionManagerHost&);

    // Separate concrete-PC profile for4243D0, with actual shared PCFX
    // construction/initialization and explicit portable ownership of field94.
    // Publish a new helper before the owner write, then reread the field for
    // the call. The projection, field and helper must remain stable/alive
    // through the synchronous material read. This does not implement a native
    // Projection/BoxProjection class, ABI or their destruction protocol.
    // Non-completed values are analytical prefix refusals; the native wrapper
    // returns literal true after any admitted whole FX initialization.
    [[nodiscard]] spProjectionFxInitializationResultForAnalysis InitializeProjectionPCWithOwnedFxForAnalysis(
        void* projection,std::unique_ptr<spPCProjectionFX>& helper94,const spProjectionFxHost&);

    class spProjectionManager : public spCrossPlatform
    {
    public:
        static constexpr spClassID ClassID=0x24A010DA;
        spProjectionManager()=default;
        ~spProjectionManager() override=default;
        spProjectionManager(const spProjectionManager&)=delete;
        spProjectionManager& operator=(const spProjectionManager&)=delete;
        [[nodiscard]] static const spRTTIRecord& StaticRTTI() noexcept;
        [[nodiscard]] const spRTTIRecord& vfunc_18() const noexcept override;
        [[nodiscard]] std::unique_ptr<spBaseObject> vfunc_10(spCloneManager&) const override;
        // Copy is the actual inherited NamedObject operation: name only.
        [[nodiscard]] virtual bool EnableForAnalysis() noexcept;
        [[nodiscard]] virtual bool InitializeProjectionsForAnalysis(spProjectionManagerHost&);
        virtual void AddProjectionForAnalysis(void* borrowedProjection);
        virtual void RemoveProjectionForAnalysis(void* borrowedProjection) noexcept;
        [[nodiscard]] virtual bool DrawPhase2CForAnalysis(void*) noexcept{return true;}
        [[nodiscard]] virtual bool DrawPhase30ForAnalysis(void*) noexcept{return true;}
        [[nodiscard]] virtual bool DrawProjectionsForAnalysis(void* borrowedCamera,spProjectionManagerHost&);
        [[nodiscard]] virtual std::uint8_t GetEnabledByteForAnalysis() const noexcept{return enabled_;}
        virtual void SetEnabledByteForAnalysis(std::uint32_t raw) noexcept{enabled_=static_cast<std::uint8_t>(raw);}
        [[nodiscard]] const std::list<void*>& GetProjectionsForAnalysis() const noexcept{return projections_;}
        // Analytical existing-state injection; it is not a recovered producer.
        // Allows duplicate/null entries, like raw native input list records.
        void SetProjectionsForFixture(std::list<void*> values);

    private:
        struct Frame final{const void* current=nullptr;bool erased=false;};
        [[nodiscard]] bool WalkForAnalysis(const std::function<std::uint8_t(void*)>&);
        std::uint8_t enabled_=0;
        std::list<void*> projections_; // borrowed payloads; host container ABI
        std::vector<Frame*> activeFrames_; // synchronous analytical erasure guard
    };
}
