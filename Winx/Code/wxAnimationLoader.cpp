#include "wxAnimationLoader.h"
#include <cstring>
#include <initializer_list>
#include <stdexcept>

namespace winx::reconstruction
{
    using namespace sparkplug::reconstruction;
    namespace
    {
        wxAnimationLoaderHost* factoryHost = nullptr;
        std::unique_ptr<spBaseObject> CreateLoader()
        {
            if (!factoryHost) throw std::logic_error("wxAnimationLoader requires a factory host");
            return std::make_unique<wxAnimationLoader>(*factoryHost);
        }
        // Registration ancestry, not substitute concrete entity implementations.
        const spRTTIRecord entityRecord{0x22875AA1, spNamedObject::ClassID, "spEntity", &spNamedObject::StaticRTTI(), nullptr, nullptr};
        const spRTTIRecord wxEntityRecord{0x796A1869, 0x22875AA1, "wxEntity", &entityRecord, nullptr, nullptr};
        const spRTTIRecord record{wxAnimationLoader::ClassID, 0x796A1869, "wxAnimationLoader", &wxEntityRecord, &CreateLoader, nullptr};
        const bool registered = spRTTIManager::Instance().RegisterDeferredForAnalysis(record);

        // Ordered set IDs for native level codes 0..50. Not a deduplicated set.
        // Level 11's extra release and the common release tail are below.
        const std::initializer_list<std::uint32_t> levelSets[] = {
            {}, // 0
            {3, 2, 5, 24, 25, 63, 64, 66}, // 1
            {5, 2, 24, 66}, // 2
            {2, 4, 5, 44, 1, 66}, // 3
            {32, 39, 41, 36, 2, 66}, // 4
            {8, 6, 2, 66}, // 5
            {9, 6, 7, 54, 2, 66}, // 6
            {32, 39, 41, 36, 54, 55, 66}, // 7
            {11, 1, 32, 39, 41, 36, 66}, // 8
            {12, 32, 36, 41, 39, 38, 30, 37, 50, 66, 64}, // 9
            {12, 14, 13, 32, 36, 41, 39, 66}, // 10
            {12, 14, 13, 32, 36, 41, 39, 27, 64, 66}, // 11
            {12, 14, 13, 32, 36, 41, 39, 27, 66, 64}, // 12
            {14, 15, 37, 30, 1, 66}, // 13
            {23, 18, 32, 36, 41, 39, 66}, // 14
            {23, 16, 32, 36, 41, 39, 66}, // 15
            {41, 29, 20, 17, 1, 66}, // 16
            {39, 27, 59, 17, 18, 66}, // 17
            {39, 27, 59, 29, 43, 57, 58, 17, 18, 66}, // 18
            {39, 27, 59, 17, 66}, // 19
            {48, 17, 29, 66}, // 20
            {48, 66}, // 21
            {29, 66}, // 22
            {53, 32, 39, 36, 41, 64, 27, 66}, // 23
            {60, 61, 62, 36, 32, 41, 39, 66}, // 24
            {62, 27, 66}, // 25
            {62, 53, 66}, // 26
            {32, 39, 36, 41, 37, 30, 38, 33, 5, 50, 64, 66}, // 27
            {32, 39, 36, 41, 37, 35, 30, 33, 5, 50, 64, 66}, // 28
            {32, 39, 36, 41, 37, 35, 30, 42, 28, 38, 5, 64, 66}, // 29
            {34, 31, 40, 28, 33, 17, 66}, // 30
            {66}, // 31
            {66}, // 32
            {37, 30, 59, 50, 27, 41, 36, 39, 32, 5, 64, 65, 66}, // 33
            {33, 43, 58, 64, 37, 35, 66}, // 34
            {32, 39, 36, 41, 50, 28, 38, 5, 57, 59, 27, 33, 29, 42, 64, 65, 66}, // 35
            {40, 31, 1}, // 36
            {1, 34}, // 37
            {51, 52}, // 38
            {51, 52}, // 39
            {51, 52}, // 40
            {66}, // 41
            {66}, // 42
            {66}, // 43
            {66}, // 44
            {66}, // 45
            {66}, // 46
            {2, 12, 14, 13, 66}, // 47
            {17, 53, 18, 16, 66}, // 48
            {17, 16, 6, 48, 66}, // 49
            {2, 66}, // 50
        };
    }

    wxAnimationLoader::wxAnimationLoader(wxAnimationLoaderHost& host) : host_(host)
    {
        host_.ConstructEntityForAnalysis(*this, true);
    }
    wxAnimationLoader::~wxAnimationLoader()
    {
        ApplyCurrentLevelForAnalysis(0);
        host_.DestroyEntityForAnalysis(*this);
    }
    void wxAnimationLoader::SetFactoryHostForAnalysis(wxAnimationLoaderHost* host) noexcept
    {
        factoryHost = host;
    }
    const spRTTIRecord& wxAnimationLoader::StaticRTTI() noexcept
    {
        (void)registered;
        return record;
    }
    const spRTTIRecord& wxAnimationLoader::vfunc_18() const noexcept { return record; }
    std::unique_ptr<spBaseObject> wxAnimationLoader::vfunc_10(spCloneManager& cm) const
    {
        auto copy = std::make_unique<wxAnimationLoader>(host_);
        cm.RegisterCloneForAnalysis(*this, *copy);
        if (!vfunc_14(*copy, cm)) return nullptr;
        return copy;
    }
    bool wxAnimationLoader::vfunc_14(spBaseObject& destination, spCloneManager& cm) const
    {
        auto* target = dynamic_cast<wxAnimationLoader*>(&destination);
        if (!target) return false; // portable guard; native requires valid type
        if (!host_.CopyEntityForAnalysis(*this, *target, cm)) return false;
        // Native clone marker is an empty PC hook. There is no own state to copy.
        return true;
    }
    void wxAnimationLoader::ApplySetForAnalysis(std::uint32_t set, std::uint8_t load) noexcept
    {
        auto* manager = host_.GetAnimationManagerForAnalysis();
        if (load) host_.LoadSetForAnalysis(manager, set);
        else host_.ReleaseSetForAnalysis(manager, set);
    }
    void wxAnimationLoader::ApplyCurrentLevelForAnalysis(std::uint8_t load) noexcept
    {
        const auto level = host_.GetCurrentLevelForAnalysis();
        if (level < sizeof(levelSets) / sizeof(levelSets[0]))
            for (const auto set : levelSets[level]) ApplySetForAnalysis(set, load);

        if (!load)
        {
            if (level == 11) ApplySetForAnalysis(21, 0);
            for (const auto set : {10u, 19u, 26u, 67u}) ApplySetForAnalysis(set, 0);
        }
    }
    void wxAnimationLoader::vfunc_0C(const void* notification) noexcept
    {
        std::uint32_t code;
        std::memcpy(&code, notification, sizeof(code));
        if (code == 0x1D) ApplyCurrentLevelForAnalysis(1);
    }
}
