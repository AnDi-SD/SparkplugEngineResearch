#pragma once
// Host ownership/registration adapter. All file and object grammar remains in
// reconstructed serializers. No renderer device, COM or filesystem calls.
#include "Code/Sparkplug/spSerializer.h"
#include "Code/Sparkplug/spNode.h"
#include "Code/SparkplugPC/spPCRenderer.h"
#include <unordered_map>

namespace spvhost {
std::shared_ptr<sparkplug::reconstruction::spPCRenderer> CpuRenderer();
class ResourceGraph final {
public:
    using Object = sparkplug::reconstruction::spBaseObject;
    using Entry = sparkplug::reconstruction::spSerializerReadContextForAnalysis::FileObjectForAnalysis;
    ResourceGraph(const std::uint8_t*,std::uint32_t);
    std::uint32_t rootID=0;
    std::vector<Entry> entries;
    std::vector<std::shared_ptr<Object>> owners;
    std::vector<std::unique_ptr<Object>> directOwners;
    std::vector<std::uint32_t> nodeIDs;
    std::shared_ptr<sparkplug::reconstruction::spPCRenderer> renderer;
    [[nodiscard]] Object* Find(std::uint32_t) const;
    [[nodiscard]] std::uint32_t ID(const Object*) const;
    [[nodiscard]] std::shared_ptr<sparkplug::reconstruction::spNode> Node(std::uint32_t) const;
private:
    std::unordered_map<std::uint32_t,Object*> byID;
    std::unordered_map<const Object*,std::uint32_t> ids;
};
struct GraphHandle {std::shared_ptr<ResourceGraph> graph;};
}
