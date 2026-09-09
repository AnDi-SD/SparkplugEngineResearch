#pragma once
#include "spNavigationSet.h"
#include "spCollisionInfo.h"
#include "spMeshBV.h"
#include <limits>
namespace sparkplug::reconstruction
{
    class spMeshNavigationSet : public spNavigationSet
    {
    public:
        static constexpr spClassID ClassID=0x7297173C;
        [[nodiscard]] static const spRTTIRecord& StaticRTTI() noexcept;
        [[nodiscard]] const spRTTIRecord& vfunc_18() const noexcept override;
        [[nodiscard]] std::unique_ptr<spBaseObject> vfunc_10(spCloneManager&) const override{return nullptr;}
        bool vfunc_14(spBaseObject&,spCloneManager&) const override{return false;}
        [[nodiscard]] spMeshBV* GetMeshForAnalysis() const noexcept{return mesh_;}
        [[nodiscard]] const spCollisionInfo& GetCollisionForAnalysis() const noexcept{return collision_;}
        [[nodiscard]] const Vector3& GetMinimumForAnalysis() const noexcept{return minimum_;}
        [[nodiscard]] const Vector3& GetMaximumForAnalysis() const noexcept{return maximum_;}
        [[nodiscard]] const std::array<float,4>& GetNavigationSphereForAnalysis() const noexcept{return sphere_;}
        bool SetMeshForAnalysis(std::shared_ptr<spMeshBV>);
    private:
        spMeshBV* mesh_=nullptr; // alias of the embedded CollisionInfo's owner
        spCollisionInfo collision_;
        Vector3 minimum_{std::numeric_limits<float>::max(),std::numeric_limits<float>::max(),std::numeric_limits<float>::max()};
        Vector3 maximum_{-std::numeric_limits<float>::max(),-std::numeric_limits<float>::max(),-std::numeric_limits<float>::max()};
        std::array<float,4> sphere_{};
    };
}
