#pragma once

// Inferred portable API. Native PC424630 is the protected constructor entry;
// its physical Renderable prefix and own fields are proven through Box.
#include "spRenderable.h"
#include <cstddef>
#include <list>
#include <optional>

namespace sparkplug::reconstruction
{
    class spPCProjectionFX;
    class spRenderNode;
    class spProjectionSerializer;
    enum class spProjectionFxInitializationResultForAnalysis;
    struct spProjectionConstructorDataForAnalysis final
    {
        // Existing common Renderable constructor's raw Debug41D4E0 result.
        // Providing data does not implement the lazy global/table sequence.
        std::uint32_t debugField28=0;
    };
    class spProjection : public spRenderable
    {
    public:
        static constexpr spClassID ClassID=0x5CB4145D;
        struct StateForAnalysis final
        {
            std::array<std::uint8_t,3> flags58{1,0,0};
            std::array<std::uint32_t,4> words5C{0x3F800000,0x3F800000,0x3F800000,0x3F800000};
            std::uint32_t word6C=2;
            BoundingSphere sphere70{};
            std::uint32_t word98=1;
        };
        ~spProjection() override;
        spProjection(const spProjection&)=delete;
        spProjection& operator=(const spProjection&)=delete;
        static const spRTTIRecord& StaticRTTI() noexcept;
        const spRTTIRecord& vfunc_18() const noexcept override;
        // The actual table inherits Renderable Copy and null Clone. It does
        // not copy own flags, sphere, list, bound84, helper94 or word98.
        static void SetFactoryConstructorDataForAnalysis(
            std::optional<spProjectionConstructorDataForAnalysis>) noexcept;
        const StateForAnalysis& GetProjectionStateForAnalysis() const noexcept{return state_;}
        std::uint32_t GetProjectionRuntimeModeForAnalysis() const noexcept{return GetRuntimeModeForAnalysis();}
        StateForAnalysis& ProjectionStateForFixture() noexcept{return state_;}
        std::size_t GetOpaqueRecordCountForAnalysis() const noexcept{return records88_.size();}
        const void* GetBound84ForAnalysis() const noexcept{return bound84_;}
        // PC468820 field5 requests RenderNode through ReadReference4678B0,
        // then stores the borrowed result, including null, without refcounts.
        // The admitted resolver must supply a real typed RenderNode; native
        // ReadReference does not itself enforce its expected-class argument.
        spRenderNode* GetBoundRenderNodeForAnalysis() const noexcept{return bound84_;}
        void SetBoundRenderNodeForAnalysis(spRenderNode* value) noexcept{bound84_=value;}
        const spPCProjectionFX* GetPCFxForAnalysis() const noexcept{return helper94_.get();}
        // Real PC4243D0 over this object's actual material relationship and
        // concrete owned helper field. No-material/initialized domains only;
        // other FX results remain explicit analytical prefix refusals.
        spProjectionFxInitializationResultForAnalysis InitializePCForAnalysis();
        const BoundingSphere& GetBoundingSphereForAnalysis() const noexcept override;
        void GetBoundsForAnalysis(BoundsPosition&,BoundsPosition&) const noexcept override;

    protected:
        explicit spProjection(spProjectionConstructorDataForAnalysis);
        static spProjectionConstructorDataForAnalysis RequireFactoryConstructorDataForAnalysis();
        virtual void RefreshProjectionSphereForAnalysis() const noexcept=0;
        virtual void ReadProjectionBoundsForAnalysis(BoundsPosition&,BoundsPosition&) const noexcept=0;
        mutable StateForAnalysis state_{};

    private:
        friend class spProjectionSerializer;
        spRenderNode* bound84_=nullptr; // borrowed serializer field5 result
        // Native empty node is140 bytes: two32-bit links plus132 opaque bytes.
        // Only the empty domain is admitted. No record fields/producer or
        // nonempty teardown semantics are inferred from this host container.
        std::list<std::array<std::byte,132>> records88_;
        std::unique_ptr<spPCProjectionFX> helper94_;
    };
}
