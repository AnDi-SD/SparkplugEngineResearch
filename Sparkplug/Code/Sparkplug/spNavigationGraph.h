#pragma once
#include "spRenderNode.h"
namespace sparkplug::reconstruction
{
    class spNavigationSet;class spNavigationPortal;
    class spNavigationGraph : public spRenderNode
    {
    public:
        static constexpr spClassID ClassID=0x188A161F;
        struct PathForAnalysis{std::uint8_t nextPortal=255;std::vector<std::array<std::uint8_t,2>> alternatives;};
        [[nodiscard]] static const spRTTIRecord& StaticRTTI() noexcept;
        [[nodiscard]] const spRTTIRecord& vfunc_18() const noexcept override;
        [[nodiscard]] std::unique_ptr<spBaseObject> vfunc_10(spCloneManager&) const override{return nullptr;}
        bool vfunc_14(spBaseObject&,spCloneManager&) const override{return false;}
        [[nodiscard]] const std::vector<spNavigationSet*>& GetSetsForAnalysis() const noexcept{return sets_;}
        [[nodiscard]] const std::vector<spNavigationPortal*>& GetPortalsForAnalysis() const noexcept{return portals_;}
        [[nodiscard]] const std::vector<std::vector<PathForAnalysis>>& GetPathsForAnalysis() const noexcept{return paths_;}
        bool AppendSetForAnalysis(spNavigationSet*);
        bool AppendPortalForAnalysis(spNavigationPortal*);
    private:
        friend class spNavigationGraphSerializer;
        std::vector<spNavigationSet*> sets_;
        std::vector<spNavigationPortal*> portals_;
        std::vector<std::vector<PathForAnalysis>> paths_;
    };
}
