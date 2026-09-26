#include "wxAnimationManager.h"
#include <stdexcept>

namespace winx::reconstruction
{
    using namespace sparkplug::reconstruction;
    namespace
    {
        wxAnimationManager* instance = nullptr;
        wxAnimationManagerHost* factoryHost = nullptr;
        std::unique_ptr<spBaseObject> CreateManager()
        {
            if (!factoryHost) throw std::logic_error("wxAnimationManager requires a factory host");
            return std::make_unique<wxAnimationManager>(*factoryHost);
        }
        const spRTTIRecord record{wxAnimationManager::ClassID, spBaseObject::ClassID,
            "wxAnimationManager", &spBaseObject::StaticRTTI(), &CreateManager, nullptr};
        const bool registered = spRTTIManager::Instance().RegisterDeferredForAnalysis(record);
        struct TokenValue { std::string_view token; std::uint32_t value; };
        constexpr TokenValue modeTokens[] = {
            {"moving", 0},
            {"flying", 1},
            {"strafing", 2},
            {"crouching", 3},
            {"hanging", 4},
            {"ladders", 6},
            {"vines", 7},
            {"sticking", 5},
            {"dialogue", 8},
            {"dating", 9},
        };
        constexpr TokenValue directionTokens[] = {
            {"none", 0},
            {"up", 1},
            {"down", 2},
            {"left", 3},
            {"right", 4},
            {"all", 5},
        };
        constexpr TokenValue actionTokens[] = {
            {"none", 0},
            {"blast", 1},
            {"missile", 2},
            {"defend", 3},
            {"kick", 4},
            {"chest", 5},
            {"stairs", 6},
            {"waytogo", 24},
            {"dispel", 25},
            {"door", 7},
            {"attack1", 8},
            {"attack2", 9},
            {"attack3", 10},
            {"attack4", 11},
            {"attack5", 12},
            {"glasses", 13},
            {"shaketree", 14},
            {"openchest", 15},
            {"action", 16},
            {"try2hoist", 17},
            {"pulllever", 19},
            {"pickflower", 20},
            {"readsign", 18},
            {"opengate", 21},
            {"opensecret", 26},
            {"neutralTalking", 28},
            {"neutralListening", 29},
            {"happyTalking", 30},
            {"happyListening", 31},
            {"angryTalking", 32},
            {"angryListening", 33},
            {"discouragedTalking", 34},
            {"discouragedListening", 35},
            {"convaincedTalking", 36},
            {"convaincedListening", 37},
            {"bedIdle", 39},
            {"bedTalking", 40},
            {"bedListening", 41},
            {"chairIdle", 42},
            {"chairTalking", 43},
            {"chairListening", 44},
            {"idleLyingDialogue", 47},
            {"lyingTalking", 45},
            {"lyingListening", 46},
            {"hoverTalking", 48},
            {"hoverListening", 49},
            {"idleHoverDialogue", 50},
            {"idleChairDialogue", 51},
            {"idleBedDialogue", 52},
            {"idleDialogue", 53},
            {"thinkingDialogue", 38},
            {"idleMedDialogue", 54},
            {"idleLong1Dialogue", 55},
            {"idleLong2Dialogue", 56},
            {"idleLong3Dialogue", 57},
            {"glyph", 22},
        };
        constexpr TokenValue statusTokens[] = {
            {"none", 0},
            {"hurt", 1},
            {"stunned", 2},
            {"dead", 3},
            {"struggling", 4},
        };
        constexpr TokenValue variantTokens[] = {
            {"0", 0},
            {"1", 1},
            {"2", 2},
            {"3", 3},
            {"4", 4},
            {"5", 5},
            {"6", 6},
            {"7", 7},
            {"8", 8},
            {"9", 9},
            {"10", 10},
            {"11", 11},
            {"12", 12},
            {"13", 13},
            {"14", 14},
            {"15", 15},
            {"16", 16},
        };
        constexpr wxAnimationManager::SetDescriptorForAnalysis setTables[] = {
            {"Bloom.anm", 1}, // 0
            {"BloomX.anm", 2}, // 1
            {"Ghoulie.anm", 5}, // 2
            {"Bird.anm", 6}, // 3
            {"Knut.anm", 7}, // 4
            {"Kiko.anm", 8}, // 5
            {"Iceworm.anm", 11}, // 6
            {"IceGargoyle.anm", 12}, // 7
            {"IceBat.anm", 6}, // 8
            {"Spirit.anm", 13}, // 9
            {nullptr, 0}, // 10
            {"Yeti.anm", 14}, // 11
            {"Mosquito.anm", 15}, // 12
            {"HungryHopper.anm", 16}, // 13
            {"HairySpider.anm", 17}, // 14
            {"Troll.anm", 18}, // 15
            {"Golem.anm", 19}, // 16
            {"Baco.anm", 20}, // 17
            {"Minotaur.anm", 21}, // 18
            {nullptr, 0}, // 19
            {"GoopMonster.anm", 24}, // 20
            {nullptr, 0}, // 21
            {nullptr, 0}, // 22
            {"Book.anm", 6}, // 23
            {"Butterfly.anm", 6}, // 24
            {"Fish.anm", 6}, // 25
            {nullptr, 0}, // 26
            {"Specialist.anm", 40}, // 27
            {"Faragonda.anm", 30}, // 28
            {"Griffin.anm", 32}, // 29
            {"WinxStudents.anm", 28}, // 30
            {"Darcy.anm", 29}, // 31
            {"WinxStudents.anm", 31}, // 32
            {"Grizelda.anm", 33}, // 33
            {"Icy.anm", 34}, // 34
            {"WinxStudents.anm", 35}, // 35
            {"WinxStudents.anm", 36}, // 36
            {"WinxStudents.anm", 37}, // 37
            {"Palladium.anm", 38}, // 38
            {"WinxStudents.anm", 41}, // 39
            {"Stormy.anm", 42}, // 40
            {"WinxStudents.anm", 43}, // 41
            {"Wizgiz.anm", 44}, // 42
            {"WinxStudents.anm", 45}, // 43
            {"XWinx.anm", 46}, // 44
            {"XWinx.anm", 47}, // 45
            {"XWinx.anm", 49}, // 46
            {"XWinx.anm", 48}, // 47
            {"Shadowbeast.anm", 52}, // 48
            {nullptr, 0}, // 49
            {"WinxStudents.anm", 39}, // 50
            {"DatingBloom.anm", 3}, // 51
            {"DatingSky.anm", 3}, // 52
            {"Droid.anm", 53}, // 53
            {"Guardian.anm", 54}, // 54
            {"Daphne.anm", 55}, // 55
            {"WinxStudents.anm", 57}, // 56
            {"WinxStudents.anm", 58}, // 57
            {"WinxStudents.anm", 59}, // 58
            {"KnutFriend.anm", 7}, // 59
            {"UpsieDaisySpider.anm", 17}, // 60
            {"Dragon.anm", 60}, // 61
            {"UpsieDaisyGuard.anm", 40}, // 62
            {"Mikael.anm", 63}, // 63
            {"WinxStudents.anm", 66}, // 64
            {"Saladin.anm", 67}, // 65
        };
        constexpr const char* levelTables[] = {
            "", // 0
            "Gardenia1Bloom.anm", // 1
            "Gardenia2Bloom.anm", // 2
            "Gardenia3Bloom.anm", // 3
            "Domino12Bloom.anm", // 4
            "Domino12Bloom.anm", // 5
            "Domino3Bloom.anm", // 6
            "Domino45Bloom.anm", // 7
            "Domino45Bloom.anm", // 8
            "SwampBloom.anm", // 9
            "SwampBloom.anm", // 10
            "SwampBloom.anm", // 11
            "SwampBloom.anm", // 12
            "SwampBloom.anm", // 13
            "CT1_1Bloom.anm", // 14
            "CT1_23Bloom.anm", // 15
            "CT1_23Bloom.anm", // 16
            "CT2_125Bloom.anm", // 17
            "CT2_125Bloom.anm", // 18
            "CT2_3Bloom.anm", // 19
            "CT2_4Bloom.anm", // 20
            "CT2_125Bloom.anm", // 21
            "CT2_125Bloom.anm", // 22
            "RF134Bloom.anm", // 23
            "RF2Bloom.anm", // 24
            "RF134Bloom.anm", // 25
            "RF134Bloom.anm", // 26
            "AlfeaBloom.anm", // 27
            "AlfeaBloom.anm", // 28
            "AlfeaLadderBloom.anm", // 29
            "AlfeaFightBloom.anm", // 30
            "AlfeaBloom.anm", // 31
            "AlfeaLadderBloom.anm", // 32
            "AlfeaBloom.anm", // 33
            "AlfeaBloom.anm", // 34
            "AlfeaLadderBloom.anm", // 35
            "SkyBloom.anm", // 36
            "SkyBloom.anm", // 37
            "", // 38
            "", // 39
            "", // 40
            "Star1Bloom.anm", // 41
            "Star2Bloom.anm", // 42
            "Star3Bloom.anm", // 43
            "Race1Bloom.anm", // 44
            "Race2Bloom.anm", // 45
            "Race3Bloom.anm", // 46
            "Battle1Bloom.anm", // 47
            "Battle2Bloom.anm", // 48
            "Battle3Bloom.anm", // 49
        };
        constexpr wxAnimationManager::PathDescriptorForAnalysis paths[] = {
            {2, 1}, // 0
            {2, 2}, // 1
            {2, 5}, // 2
            {2, 6}, // 3
            {2, 7}, // 4
            {2, 8}, // 5
            {2, 11}, // 6
            {2, 12}, // 7
            {2, 6}, // 8
            {2, 13}, // 9
            {12, 0}, // 10
            {2, 14}, // 11
            {2, 15}, // 12
            {2, 16}, // 13
            {2, 17}, // 14
            {2, 18}, // 15
            {2, 19}, // 16
            {2, 20}, // 17
            {2, 21}, // 18
            {14, 0}, // 19
            {2, 24}, // 20
            {2, 26}, // 21
            {4, 0}, // 22
            {2, 6}, // 23
            {2, 6}, // 24
            {2, 6}, // 25
            {4, 0}, // 26
            {2, 40}, // 27
            {2, 30}, // 28
            {2, 32}, // 29
            {2, 28}, // 30
            {2, 29}, // 31
            {2, 31}, // 32
            {2, 33}, // 33
            {2, 34}, // 34
            {2, 35}, // 35
            {2, 36}, // 36
            {2, 37}, // 37
            {2, 38}, // 38
            {2, 41}, // 39
            {2, 42}, // 40
            {2, 43}, // 41
            {2, 44}, // 42
            {2, 45}, // 43
            {2, 46}, // 44
            {2, 47}, // 45
            {2, 49}, // 46
            {2, 48}, // 47
            {2, 52}, // 48
            {14, 0}, // 49
            {2, 39}, // 50
            {2, 3}, // 51
            {2, 3}, // 52
            {2, 53}, // 53
            {2, 54}, // 54
            {2, 55}, // 55
            {2, 57}, // 56
            {2, 58}, // 57
            {2, 59}, // 58
            {2, 7}, // 59
            {2, 17}, // 60
            {2, 60}, // 61
            {2, 40}, // 62
            {2, 63}, // 63
            {2, 66}, // 64
            {2, 67}, // 65
            {2, 1}, // 66
            {4, 0}, // 67
        };
        template<std::size_t N>
        std::uint32_t Parse(const TokenValue (&table)[N], std::string_view text) noexcept
        {
            text = text.substr(0, text.find('\0'));
            for (const auto& entry : table) if (entry.token == text) return entry.value;
            return 0;
        }
        void RequireTable(std::uint32_t table)
        {
            if (table >= wxAnimationManager::TableCount)
                throw std::out_of_range("wxAnimationManager table index"); // portable invalid-input guard
        }
    }
    wxAnimationManager::wxAnimationManager(wxAnimationManagerHost& host) : host_(host) { instance = this; }
    wxAnimationManager::~wxAnimationManager() { ClearForAnalysis(); instance = nullptr; }
    void wxAnimationManager::SetFactoryHostForAnalysis(wxAnimationManagerHost* host) noexcept { factoryHost = host; }
    wxAnimationManager* wxAnimationManager::GetInstance() noexcept { return instance; }
    const spRTTIRecord& wxAnimationManager::StaticRTTI() noexcept { (void)registered; return record; }
    const spRTTIRecord& wxAnimationManager::vfunc_18() const noexcept { return record; }
    std::unique_ptr<spBaseObject> wxAnimationManager::vfunc_10(spCloneManager& cm) const
    {
        auto copy = std::make_unique<wxAnimationManager>(host_);
        cm.RegisterCloneForAnalysis(*this, *copy);
        if (!vfunc_14(*copy, cm)) return nullptr;
        return copy;
    }
    std::uint32_t wxAnimationManager::ParseTokenForAnalysis(TokenForAnalysis kind, std::string_view text) noexcept
    {
        switch (kind)
        {
        case TokenForAnalysis::Mode: return Parse(modeTokens, text);
        case TokenForAnalysis::Direction: return Parse(directionTokens, text);
        case TokenForAnalysis::Action: return Parse(actionTokens, text);
        case TokenForAnalysis::Status: return Parse(statusTokens, text);
        case TokenForAnalysis::Variant: return Parse(variantTokens, text);
        }
        return 0;
    }
    std::uint32_t wxAnimationManager::PackKeyForAnalysis(const Row& row) noexcept
    {
        const auto jump = row[4] == "jumping" ? 1u : row[4] == "falling" ? 2u : 0u;
        const auto phase = row[5] == "enter" ? 1u : row[5] == "exit" ? 2u : 0u;
        return (Parse(modeTokens, row[0]) & 15u) | ((Parse(directionTokens, row[1]) & 7u) << 4)
            | ((Parse(actionTokens, row[2]) & 255u) << 7) | ((Parse(statusTokens, row[3]) & 15u) << 15)
            | (jump << 19) | (phase << 21) | ((Parse(variantTokens, row[6]) & 31u) << 23);
    }
    wxAnimationManager::SetDescriptorForAnalysis wxAnimationManager::DescribeSetForAnalysis(std::uint32_t set, std::uint32_t level) noexcept
    {
        if (set == 66) return {level < 50 ? levelTables[level] : "", 1};
        if (set < 66) return setTables[set];
        return {nullptr, 0};
    }
    wxAnimationManager::PathDescriptorForAnalysis wxAnimationManager::DescribePathForAnalysis(std::uint32_t set) noexcept
    {
        return set < TableCount ? paths[set] : PathDescriptorForAnalysis{2, 0};
    }
    wxAnimationManager::Resource wxAnimationManager::GetAnimationForAnalysis(std::string_view name, std::uint32_t set)
    {
        const auto descriptor = DescribePathForAnalysis(set);
        auto path = host_.ResolveAnimationPathForAnalysis(name, descriptor.category, descriptor.group, 0x136);
        path.resize(path.find('\0') == std::string::npos ? path.size() : path.find('\0'));
        if (const auto found = cache_.find(path); found != cache_.end()) return found->second.resource;
        auto* resource = host_.LoadAnimationForAnalysis(path);
        cache_[path] = {resource, set}; // null is also cached; first owner's ID survives cache hits
        return resource;
    }
    void wxAnimationManager::LoadSetForAnalysis(std::uint32_t set)
    {
        const auto descriptor = DescribeSetForAnalysis(set, set == 66 ? host_.GetCurrentLevelForAnalysis() : 0);
        if (!descriptor.name) return;
        auto* source = host_.OpenTableForAnalysis(descriptor.name, 10, descriptor.group, 0);
        if (!source) return;
        void* parser = nullptr;
        try
        {
            parser = host_.CreateTokenStreamForAnalysis(source);
            constexpr std::size_t capacity[] = {20, 20, 40, 20, 10, 20, 5, 20};
            for (;;)
            {
                Row row;
                for (std::size_t i = 0; i < row.size(); ++i)
                    row[i] = host_.ReadTokenForAnalysis(parser, capacity[i], i == 7 ? ';' : ',');
                if (row[0] == "end") break;
                const auto key = PackKeyForAnalysis(row);
                tables_[set][key] = GetAnimationForAnalysis(row[7], set);
            }
        }
        catch (...)
        {
            // Portable exception cleanup; not a recovered native failure path.
            host_.CloseAndDestroyStreamForAnalysis(source);
            if (parser) host_.CloseAndDestroyStreamForAnalysis(parser);
            throw;
        }
        host_.CloseAndDestroyStreamForAnalysis(source);
        host_.CloseAndDestroyStreamForAnalysis(parser);
    }
    void wxAnimationManager::ReleaseSetForAnalysis(std::uint32_t set)
    {
        RequireTable(set);
        for (auto it = cache_.begin(); it != cache_.end();)
        {
            if (it->second.owner != set) { ++it; continue; }
            if (it->second.resource) host_.DestroyAnimationForAnalysis(it->second.resource);
            it = cache_.erase(it);
        }
        tables_[set].clear(); // references in other tables are deliberately unchanged
    }
    void wxAnimationManager::ClearForAnalysis() noexcept
    {
        for (auto& entry : cache_)
            if (entry.second.resource) host_.DestroyAnimationForAnalysis(entry.second.resource);
        cache_.clear();
        for (auto& table : tables_) table.clear();
    }
    wxAnimationManager::Resource wxAnimationManager::SelectForAnalysis(std::uint32_t key, std::uint32_t table) const
    {
        RequireTable(table);
        const auto& primary = tables_[table];
        if (const auto found = primary.find(key); found != primary.end()) return found->second;
        if (table == 0)
            if (const auto found = tables_[66].find(key); found != tables_[66].end()) return found->second;
        if (const auto fallback = primary.find(0); fallback != primary.end()) return fallback->second;
        // Native dereferences its end iterator; no successful fallback value is known.
        throw std::logic_error("wxAnimationManager missing mandatory default key");
    }
}
