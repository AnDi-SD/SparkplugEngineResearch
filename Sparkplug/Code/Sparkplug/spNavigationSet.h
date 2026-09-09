#pragma once
#include "spNode.h"
#include "Analysis/PC/spNavigationStorage.h"
namespace sparkplug::reconstruction
{
    class spNavigationPortal;
    class spNavigationSet : public spNode
    {
    public:
        static constexpr spClassID ClassID=0x74F9013E;
        using Table=evidence::pc::NavigationTransitionTableForAnalysis;
        [[nodiscard]] static const spRTTIRecord& StaticRTTI() noexcept;
        [[nodiscard]] const spRTTIRecord& vfunc_18() const noexcept override;
        [[nodiscard]] std::unique_ptr<spBaseObject> vfunc_10(spCloneManager&) const override{return nullptr;}
        bool vfunc_14(spBaseObject&,spCloneManager&) const override{return false;}
        // Original base registration has no factory; concrete Mesh set supplies it.
        [[nodiscard]] std::uint32_t GetNodeCountForAnalysis() const noexcept{return nodeCount_;}
        [[nodiscard]] const Table* GetTransitionsForAnalysis() const noexcept{return transitions_.get();}
        [[nodiscard]] const Table* GetPortalTransitionsForAnalysis() const noexcept{return portalTransitions_.get();}
        [[nodiscard]] const std::vector<std::vector<std::uint8_t>>& GetNeighboursForAnalysis() const noexcept{return neighbours_;}
        [[nodiscard]] const std::vector<spNavigationPortal*>& GetPortalsForAnalysis() const noexcept{return portals_;}
        [[nodiscard]] std::uint8_t GetNavigationEnabledForAnalysis() const noexcept{return enabled_;}
        [[nodiscard]] std::uint8_t GetGraphIndexForAnalysis() const noexcept{return index_;}
        void SetGraphIndexForAnalysis(std::uint8_t value) noexcept{index_=value;}
        bool SetNodeCountForAnalysis(std::uint32_t);
        bool AppendNeighbourForAnalysis(std::uint8_t source,std::uint8_t destination);
        bool AppendPortalForAnalysis(spNavigationPortal*);
    protected:
        spNavigationSet() noexcept=default;
    private:
        friend class spNavigationSetSerializer;
        std::uint32_t nodeCount_=0;
        std::unique_ptr<Table> transitions_,portalTransitions_;
        std::vector<std::vector<std::uint8_t>> neighbours_;
        std::vector<spNavigationPortal*> portals_; // borrowed, repeated occurrences
        std::uint8_t index_=255,enabled_=1;
    };
}
