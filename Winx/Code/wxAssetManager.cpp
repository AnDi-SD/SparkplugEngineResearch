#include "wxAssetManager.h"
#include <array>
#include <stdexcept>
#include <utility>

namespace winx::reconstruction
{
    using namespace sparkplug::reconstruction;
    namespace
    {
        wxAssetManagerHost* factoryHost = nullptr;
        wxAssetManager* instance = nullptr;
        std::unique_ptr<spBaseObject> Create()
        {
            if (!factoryHost) throw std::logic_error("wxAssetManager requires a factory host");
            return std::make_unique<wxAssetManager>(*factoryHost);
        }
        const spRTTIRecord record{wxAssetManager::ClassID, spBaseObject::ClassID,
            "wxAssetManager", &spBaseObject::StaticRTTI(), &Create, nullptr};
        const bool registered = spRTTIManager::Instance().RegisterDeferredForAnalysis(record);

        // Indexes and duplicate entries are part of the native path contract.
        constexpr std::array<std::string_view, 19> categories{
            "", "Levels\\", "Characters\\", "Sounds\\", "Menus\\", "Movies\\",
            "Music\\", "Voices\\", "", "Scripts\\", "Characters\\", "Characters\\",
            "Animations\\", "Fonts\\", "SFX\\", "Sounds\\", "Saved\\",
            "OneLiners\\", "Icons\\"
        };
        constexpr std::array<std::string_view, 69> subcategories{
            "", "Bloom\\", "BloomX\\", "DatingAssets\\", "Gardenia\\", "Ghoul\\",
            "Animals\\", "Knut\\", "Kiko\\", "Domino\\", "Swamp\\", "IceWorm\\",
            "IceGargoyle\\", "Spirit\\", "Yeti\\", "Mosquito\\", "HungryHopper\\",
            "HairySpider\\", "Troll\\", "Golem\\", "Baco\\", "Minautor\\",
            "Cloud01\\", "Cloud02\\", "BigGoop\\", "Alfea\\", "QuietusCarnivorous\\",
            "Hud\\", "Bloom\\", "Darcy\\", "Faragonda\\", "Bloom\\",
            "Griffin\\", "Grizelda\\", "Icy\\", "Bloom\\", "Bloom\\", "Bloom\\",
            "Palladium\\", "Bloom\\", "Specialists\\", "Bloom\\", "Stormy\\",
            "Bloom\\", "Wizgiz\\", "Bloom\\", "BloomX\\", "BloomX\\", "BloomX\\",
            "BloomX\\", "Library\\", "Location\\", "Shadowbeast\\", "Droid\\",
            "Guardian\\", "Spirit\\", "RedF\\", "Diaspro\\", "Bloom\\", "Bloom\\",
            "Dragon\\", "Sky\\", "SideQuestObject\\", "Specialists\\",
            "Challenges\\", "faces\\", "Bloom\\", "Saladin\\", "SUBTITLE\\"
        };
        constexpr std::string_view RegistryKey = "Software\\Konami\\Winx Club";
        std::string_view CString(std::string_view value) noexcept
        { return value.substr(0, value.find('\0')); }
    }

    wxAssetManager::wxAssetManager(wxAssetManagerHost& host) : host_(host) { instance = this; }
    wxAssetManager::~wxAssetManager() { instance = nullptr; }
    void wxAssetManager::SetFactoryHostForAnalysis(wxAssetManagerHost* host) noexcept { factoryHost = host; }
    wxAssetManager* wxAssetManager::GetInstanceForAnalysis() noexcept { return instance; }
    const spRTTIRecord& wxAssetManager::StaticRTTI() noexcept { (void)registered; return record; }
    const spRTTIRecord& wxAssetManager::vfunc_18() const noexcept { return record; }
    std::unique_ptr<spBaseObject> wxAssetManager::vfunc_10(spCloneManager& cm) const
    {
        auto copy = std::make_unique<wxAssetManager>(host_);
        cm.RegisterCloneForAnalysis(*this, *copy);
        if (!vfunc_14(*copy, cm)) return nullptr;
        return copy;
    }
    void wxAssetManager::SetLanguageForAnalysis(std::uint32_t mode)
    {
        languageMode_ = mode;
        switch (mode)
        {
        case 1: languageDirectory_ = "French\\"; break;
        case 2: languageDirectory_ = "German\\"; break;
        case 3: languageDirectory_ = "Italian\\"; break;
        case 4: languageDirectory_ = "Spanish\\"; break;
        default: languageDirectory_ = "English\\"; break;
        }
    }
    void wxAssetManager::InitializeForAnalysis()
    {
        SetLanguageForAnalysis(host_.AppLanguageForAnalysis());
        if (host_.IsPs2ForAnalysis())
            root_ = "data\\";
        else
        {
            auto mediaPath = host_.ReadPcRegistryValueForAnalysis(RegistryKey, "MediaPath");
            if (!mediaPath || mediaPath->size() >= 0x12c)
                throw std::runtime_error("wxAssetManager MediaPath is absent or too long");
            root_ = std::string(CString(*mediaPath));
            if (auto language = host_.ReadPcRegistryValueForAnalysis(RegistryKey, "LanguageID"))
            {
                std::uint32_t mode = 0;
                if (!language->empty() && language->size() < 0x12c)
                {
                    const auto id = CString(*language);
                    if (id == "1036") mode = 1;
                    else if (id == "7") mode = 2;
                    else if (id == "16") mode = 3;
                    else if (id == "10") mode = 4;
                    host_.SetAppLanguageForAnalysis(mode);
                    SetLanguageForAnalysis(mode);
                }
            }
        }
        catalogReady_ = true;
    }
    bool wxAssetManager::BuildPathForAnalysis(std::string& output, std::size_t capacity,
        std::uint32_t category, std::uint32_t subcategory, std::string_view filename) const
    {
        if (!catalogReady_ || !root_ || category >= categories.size() ||
            subcategory >= subcategories.size()) return false;
        std::string path = *root_;
        path += categories[category];
        if (!(category == 3 && subcategory == 0x33)) path += subcategories[subcategory];
        if (category == 17 || category == 7 ||
            ((category == 5 || category == 13) && subcategory != 0x44) ||
            (category == 3 && subcategory == 0x33))
            path += languageDirectory_;
        path += filename;
        if (path.size() >= capacity) return false;
        output = std::move(path);
        return true;
    }
    std::unique_ptr<wxAssetFileStreamForAnalysis> wxAssetManager::OpenFileStreamForAnalysis(
        std::uint32_t category, std::uint32_t subcategory,
        std::string_view filename, bool secondMode)
    {
        std::string path;
        if (!BuildPathForAnalysis(path, 0x12c, category, subcategory, filename)) return nullptr;
        auto stream = host_.CreateFileStreamForAnalysis();
        if (!stream) return nullptr;
        if (!stream->OpenForAnalysis(path, secondMode ? 2u : 1u))
        {
            host_.ReportOpenFailureForAnalysis(path);
            return nullptr;
        }
        return stream;
    }
    std::string_view wxAssetManager::CategoryForAnalysis(std::size_t i) noexcept
    { return i < categories.size() ? categories[i] : std::string_view{}; }
    std::string_view wxAssetManager::SubcategoryForAnalysis(std::size_t i) noexcept
    { return i < subcategories.size() ? subcategories[i] : std::string_view{}; }
}
