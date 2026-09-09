#pragma once
#include "spNode.h"
namespace sparkplug::reconstruction
{
    class spNavigationGraph;class spNavigationSet;
    class spNavigationPortal : public spNode
    {
    public:
        static constexpr spClassID ClassID=0x385662AA;
        [[nodiscard]] static const spRTTIRecord& StaticRTTI() noexcept;
        [[nodiscard]] const spRTTIRecord& vfunc_18() const noexcept override;
        [[nodiscard]] std::unique_ptr<spBaseObject> vfunc_10(spCloneManager&) const override{return nullptr;}
        bool vfunc_14(spBaseObject&,spCloneManager&) const override{return false;}
        [[nodiscard]] bool IsGraphKnownForAnalysis() const noexcept{return graphKnown_;}
        [[nodiscard]] bool AreEndpointsKnownForAnalysis() const noexcept{return endpointsKnown_;}
        [[nodiscard]] spNavigationGraph* GetGraphForAnalysis() const noexcept{return graph_;}
        [[nodiscard]] const std::array<spNavigationSet*,2>& GetEndpointsForAnalysis() const noexcept{return endpoints_;}
        [[nodiscard]] const std::vector<std::uint8_t>& GetFirstNodesForAnalysis() const noexcept{return firstNodes_;}
        [[nodiscard]] const std::vector<std::uint8_t>& GetSecondNodesForAnalysis() const noexcept{return secondNodes_;}
        [[nodiscard]] const std::vector<std::array<std::uint8_t,3>>& GetPathsForAnalysis() const noexcept{return paths_;}
        [[nodiscard]] std::uint8_t GetGraphIndexForAnalysis() const noexcept{return index_;}
        [[nodiscard]] std::uint8_t GetNavigationEnabledForAnalysis() const noexcept{return enabled_;}
        void SetGraphIndexForAnalysis(std::uint8_t value) noexcept{index_=value;}
    private:
        friend class spNavigationPortalSerializer;
        // Original constructor leaves these three pointers untouched. Null is
        // safe portable backing; explicit known flags prevent an invented default.
        bool graphKnown_=false,endpointsKnown_=false;
        spNavigationGraph* graph_=nullptr;
        std::array<spNavigationSet*,2> endpoints_{};
        std::vector<std::uint8_t> firstNodes_,secondNodes_;
        std::vector<std::array<std::uint8_t,3>> paths_;
        std::uint8_t enabled_=1,index_=255;
    };
}
