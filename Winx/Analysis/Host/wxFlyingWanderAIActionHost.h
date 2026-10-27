#pragma once
#include <array>
#include <cstdint>

namespace sparkplug::reconstruction { class spNode; }
namespace winx::reconstruction
{
    class wxPerception;
    class wxFlyingWanderAIActionHost
    {
    public:
        virtual ~wxFlyingWanderAIActionHost()=default;
        // Raw mutable PC7600E0 triple. Native constructor reads it twice;
        // its original semantic name is unknown. It is not a Node default.
        virtual std::array<std::uint32_t,3> ReadDefaultVectorWordsForAnalysis() noexcept=0;
        // Already constructed stable root, with null Scene link. The root and
        // this host must outlive all actions using this bounded PC lifetime.
        virtual sparkplug::reconstruction::spNode& ReadSceneRootForAnalysis() noexcept=0;
        virtual std::uint8_t ReadOwnerPermissionForAnalysis(void* owner) noexcept=0;
        virtual wxPerception& ReadPerceptionForAnalysis(void* owner) noexcept=0;
        // Actual selector entry receiver/key/parameter. Delivery is separate.
        virtual void SelectActionForAnalysis(void* owner,std::uint32_t key,std::uint32_t parameter) noexcept=0;
    };
}
