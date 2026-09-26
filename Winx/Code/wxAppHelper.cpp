#include "wxAppHelper.h"
#include <algorithm>
#include <limits>
#include <stdexcept>

namespace winx::reconstruction
{
    using namespace sparkplug::reconstruction;
    namespace
    {
        wxAppHelper* instance = nullptr;
        wxAppHelperHost* factoryHost = nullptr;
        std::unique_ptr<spBaseObject> Create()
        {
            if (!factoryHost) throw std::logic_error("wxAppHelper requires a factory host");
            return std::make_unique<wxAppHelper>(*factoryHost);
        }
        const spRTTIRecord record{wxAppHelper::ClassID, spBaseObject::ClassID,
            "wxAppHelper", &spBaseObject::StaticRTTI(), &Create, nullptr};
        const bool registered = spRTTIManager::Instance().RegisterDeferredForAnalysis(record);
        std::string_view CString(std::string_view s) { return s.substr(0, s.find('\0')); }
        bool Equal(std::string_view a, std::string_view b)
        {
            a = CString(a);
            if (a.size() != b.size()) return false;
            for (std::size_t i = 0; i < a.size(); ++i)
            {
                auto c = static_cast<unsigned char>(a[i]);
                if (c >= 'A' && c <= 'Z') c += 'a' - 'A';
                auto d = static_cast<unsigned char>(b[i]);
                if (d >= 'A' && d <= 'Z') d += 'a' - 'A';
                if (c != d) return false;
            }
            return true;
        }
        std::uint32_t Decimal(std::string_view s)
        {
            s = CString(s); std::size_t i = 0;
            while (i < s.size() && (s[i] == ' ' || (s[i] >= '\t' && s[i] <= '\r'))) ++i;
            const bool negative = i < s.size() && s[i] == '-';
            if (i < s.size() && (s[i] == '+' || s[i] == '-')) ++i;
            std::uint64_t value = 0, limit = negative ? 0x80000000ull : 0x7fffffffull;
            for (; i < s.size() && s[i] >= '0' && s[i] <= '9'; ++i)
                value = std::min(limit, value * 10 + static_cast<unsigned>(s[i] - '0'));
            return negative ? 0u - static_cast<std::uint32_t>(value) : static_cast<std::uint32_t>(value);
        }
    }
    wxAppHelper::wxAppHelper(wxAppHelperHost& host) : host_(host) { instance = this; }
    wxAppHelper::~wxAppHelper() { instance = nullptr; }
    void wxAppHelper::SetFactoryHostForAnalysis(wxAppHelperHost* host) noexcept { factoryHost = host; }
    wxAppHelper* wxAppHelper::GetInstance() noexcept { return instance; }
    wxAppHelper& wxAppHelper::Current()
    {
        if (!instance) throw std::logic_error("wxAppHelper singleton must be resolved by the application");
        return *instance;
    }
    const spRTTIRecord& wxAppHelper::StaticRTTI() noexcept { (void)registered; return record; }
    const spRTTIRecord& wxAppHelper::vfunc_18() const noexcept { return record; }
    std::unique_ptr<spBaseObject> wxAppHelper::vfunc_10(spCloneManager& cm) const
    {
        auto copy = std::make_unique<wxAppHelper>(host_);
        cm.RegisterCloneForAnalysis(*this, *copy);
        if (!vfunc_14(*copy, cm)) return nullptr;
        return copy;
    }
    bool wxAppHelper::ParseBooleanForAnalysis(std::string_view s) noexcept
    { return !s.empty() && (s.front() == 't' || s.front() == 'T'); }
    std::uint32_t wxAppHelper::ParseLanguageForAnalysis(std::string_view s) noexcept
    {
        constexpr const char* names[] = {"en", "fr", "de", "it", "es"};
        for (std::uint32_t i = 0; i < 5; ++i) if (Equal(s, names[i])) return i;
        return 0;
    }
    std::uint32_t wxAppHelper::ParseDisplayTypeForAnalysis(std::string_view s) noexcept
    { return Equal(s, "PAL") ? 1u : Equal(s, "NTSC") ? 2u : 0u; }
    std::uint32_t wxAppHelper::ParseTerritoryForAnalysis(std::string_view s) noexcept
    { return Equal(s, "eu") ? 1u : Equal(s, "na") ? 2u : 0u; }
    const wxAppHelper::Configuration& wxAppHelper::GetConfigurationForAnalysis() const
    {
        if (!config_) throw std::logic_error("wxAppHelper settings have not been initialized");
        return *config_;
    }
    void wxAppHelper::LoadConfigurationForAnalysis()
    {
        config_ = Configuration{};
        auto text = host_.ReadConfigurationForAnalysis("winx.ini");
        if (text) ParseConfigurationForAnalysis(*text);
    }
    void wxAppHelper::ParseConfigurationForAnalysis(std::string_view bytes)
    {
        config_ = Configuration{};
        auto& c = *config_;
        const auto size = bytes.size(); bytes = CString(bytes);
        std::size_t cursor = 0, remaining = size;
        while (cursor < bytes.size())
        {
            cursor = bytes.find_first_not_of("\n\r", cursor);
            if (cursor == bytes.npos || remaining <= 5) break;
            const auto end = bytes.find_first_of("\n\r", cursor);
            const auto line = bytes.substr(cursor, end == bytes.npos ? end : end - cursor);
            remaining = size - cursor - line.size();
            const auto equal = line.find('=');
            if (equal != line.npos && equal + 1 < line.size())
            {
                // Native strncpy precedes its key-length test; reject overflow explicitly.
                if (equal >= 100) throw std::length_error("wxAppHelper native INI key buffer");
                auto key = line.substr(0, equal), value = line.substr(equal + 1);
                if (equal + 1 < 101 && value.size() < 101)
                {
                    const bool flag = ParseBooleanForAnalysis(value);
                    if (Equal(key, "showCinematics")) c.showCinematics = flag;
                    else if (Equal(key, "useGamePad")) c.useGamePad = flag;
                    else if (Equal(key, "fullScreen")) c.fullScreen = flag;
                    else if (Equal(key, "enableSound")) c.enableSound = flag;
                    else if (Equal(key, "enableDialog")) c.enableDialog = flag;
                    else if (Equal(key, "enableShadows")) c.enableShadows = flag;
                    else if (Equal(key, "loadFromPCK")) c.loadFromPCK = flag;
                    else if (Equal(key, "buildPCK")) c.buildPCK = flag;
                    else if (Equal(key, "loadFromCD")) c.loadFromCD = flag;
                    else if (Equal(key, "displayType")) c.displayType = ParseDisplayTypeForAnalysis(value);
                    else if (Equal(key, "language")) c.language = ParseLanguageForAnalysis(value);
                    else if (Equal(key, "territory")) c.territory = ParseTerritoryForAnalysis(value);
                    else if (Equal(key, "firstPCKLevel")) c.firstPCKLevel = Decimal(value);
                    else if (Equal(key, "startLevel")) c.startLevel = Decimal(value);
                    else if (Equal(key, "testCinematic")) c.testCinematic = flag;
                    else if (Equal(key, "cinematicToTest")) c.cinematicToTest = Decimal(value);
                }
            }
            if (end == bytes.npos) break;
            cursor = end + 1;
        }
    }
    wxAppHelper::Configuration wxAppHelper::PS2ConfigurationForAnalysis(std::uint32_t language) noexcept
    {
        Configuration c;
        c.useGamePad = c.enableShadows = c.loadFromPCK = c.loadFromCD = true;
        c.displayType = c.territory = 1;
        switch (language) { case 2:c.language=1;break;case 3:c.language=4;break;
            case 4:c.language=2;break;case 5:c.language=3;break;default:c.language=0; }
        return c;
    }
    wxAppHelper::Handle wxAppHelper::Call(std::uint32_t op, std::uint32_t service, std::initializer_list<Handle> args)
    { return host_.InvokeForAnalysis(op, service ? Service(service) : 0, args); }
    void wxAppHelper::FormatPackagePathForAnalysis(std::string_view name, std::string& output)
    {
        name = CString(name);
        if (name.size() < 3) throw std::invalid_argument("wxAppHelper native package name requires three readable bytes");
        std::string relative(name);
        if (name[2] >= '1' && name[2] <= '9') relative = std::string(1, name[2]) + "\\" + relative;
        if (relative.size() >= 128) throw std::length_error("wxAppHelper native package buffer");
        if (Current().GetConfigurationForAnalysis().buildPCK) output = "DATA\\PCK\\" + relative + ".txt";
        if (Current().GetConfigurationForAnalysis().loadFromPCK) output = "DATA\\PCK\\" + relative + ".pck";
    }
    void wxAppHelper::OpenPackageForAnalysis(std::string_view name, std::uint32_t mode)
    {
        std::string path; FormatPackagePathForAnalysis(name, path);
        Service(0x75529c);
        if (Current().GetConfigurationForAnalysis().buildPCK)
            host_.InvokeTextForAnalysis(0x45b950, Service(0x75db9c), path);
        if (Current().GetConfigurationForAnalysis().loadFromPCK)
            host_.InvokeTextForAnalysis(0x8000001c, Service(0x75db9c), path, {mode});
        Service(0x75529c);
    }
    void wxAppHelper::ClosePackageForAnalysis(std::string_view name)
    {
        Service(0x75529c);
        std::string path; FormatPackagePathForAnalysis(name, path);
        if (Current().GetConfigurationForAnalysis().buildPCK) Call(0x45b9d0, 0x75db9c);
        if (Current().GetConfigurationForAnalysis().loadFromPCK)
            host_.InvokeTextForAnalysis(0x45bf10, Service(0x75db9c), path);
        Service(0x75529c);
    }
    void wxAppHelper::LoadResourceTreesForAnalysis(const std::vector<std::string>& names)
    {
        auto open = [&](std::string_view name) { return host_.InvokeTextForAnalysis(0x592e00, Service(0x755280), name, {13,0,0}); };
        if (Current().GetConfigurationForAnalysis().buildPCK)
        {
            auto old = host_.ReadWordForAnalysis(Service(0x755280), 0x18);
            for (std::uint32_t language = 0; language < 5; ++language)
            {
                Call(0x592b70, 0x755280, {language});
                for (auto& name : names) if (auto stream = open(name)) host_.InvokeVirtualForAnalysis(0, stream, {1});
            }
            Call(0x592b70, 0x755280, {old});
        }
        for (auto& name : names) if (auto stream = open(name))
        {
            auto loader = host_.InvokeForAnalysis(0x419e10, 0, {stream});
            auto tree = host_.InvokeForAnalysis(0x422b50, loader);
            if (tree) Call(0x41fce0, 0x755270, {tree});
            host_.InvokeVirtualForAnalysis(0, stream, {1});
        }
    }
    void wxAppHelper::CheckOneLinersForAnalysis()
    {
        if (!Current().GetConfigurationForAnalysis().buildPCK) return;
        auto old = host_.ReadWordForAnalysis(Service(0x755280), 0x18);
        for (std::uint32_t language = 0; language < 5; ++language)
        {
            Call(0x592b70, 0x755280, {language});
            auto package = "AlfeaOL" + std::to_string(host_.ReadWordForAnalysis(Service(0x755280), 0x18));
            OpenPackageForAnalysis(package, 0);
            for (std::uint32_t i = 0; i < 918; ++i) if (host_.GetAssetKindForAnalysis(i) == 79)
            {
                auto name = host_.GetAssetNameForAnalysis(i);
                Service(0x755280);
                auto path = host_.ResolveAssetPathForAnalysis(name, 17, 0, 500);
                if (!host_.CheckFileForAnalysis(path)) host_.ReportMissingFileForAnalysis(path);
            }
            ClosePackageForAnalysis(package);
        }
        Call(0x592b70, 0x755280, {old});
    }
    void wxAppHelper::BuildInGamePackageForAnalysis()
    {
        if (!Current().GetConfigurationForAnalysis().buildPCK) return;
        OpenPackageForAnalysis("InGame", 0);
        Call(0x5d45e0, 0); Call(0x5d2640, 0);
        ClosePackageForAnalysis("InGame");
    }
    void wxAppHelper::GameSlot(std::uint32_t field, std::uint32_t slot)
    { host_.InvokeVirtualForAnalysis(slot, host_.ReadHandleForAnalysis(Service(0x755294), field)); }
    void wxAppHelper::ResetMenuForAnalysis()
    {
        for (auto field : {0xf4u,0xf8u,0xfcu,0x104u}) GameSlot(field, 0x3c);
        runtime_.menuReady = false;
    }
    bool wxAppHelper::ResetSessionForAnalysis()
    { Call(0x4e25f0, 0x765ad8); Call(0x5840d0, 0); return true; }

    void wxAppHelper::PrepareSceneStorage()
    {
        auto prepare = [&](Handle scene, bool small)
        {
            if (!scene) return;
            auto node = host_.InvokeForAnalysis(0x45d930, scene);
            auto storage = host_.ReadHandleForAnalysis(node, 0x1d4);
            host_.InvokeForAnalysis(0x576660, host_.InteriorForAnalysis(storage, 0x20), {small ? 16u : 1024u});
            if (!small) host_.InvokeForAnalysis(0x576580, host_.InteriorForAnalysis(storage, 0x10), {256});
        };
        prepare(host_.ReadHandleForAnalysis(Service(0x755274), 0x18), false);
        prepare(host_.ReadHandleForAnalysis(host_.ResolveForAnalysis(0x75db8c, false), 0x18), false);
        prepare(host_.ReadHandleForAnalysis(Service(0x755284), 0x2d4), true);
    }
    bool wxAppHelper::InitializeForAnalysis()
    {
        runtime_.active = false;
        Call(0x593240, 0x755280);
        OpenPackageForAnalysis("Init", 1);
        if (!Current().GetConfigurationForAnalysis().buildPCK)
        {
            Call(0x417c50, 0x75ac60);
            OpenPackageForAnalysis("LOADSCRN", 1); OpenPackageForAnalysis("BLOOM", 2);
            OpenPackageForAnalysis("INGAME", 1); OpenPackageForAnalysis("SUBTITLE", 1);
            Call(0x417c50, 0x75ac60);
        }
        runtime_.field14 = host_.InteriorForAnalysis(Service(0x75529c), 0x58);
        runtime_.field18 = host_.InteriorForAnalysis(Service(0x75529c), 0x58);
        Call(0x4073a0, 0); Call(0x407400, 0); Call(0x5a10a0, 0);
        Call(0x5778c0, 0x765ae8); Service(0x765b68);
        Call(0x458b80, 0x75db78, {1,2500}); Call(0x5d65c0, 0x764e8c, {1,1024});
        Service(0x765bf4); Service(0x765c08);
        host_.WriteWordForAnalysis(0, 0x741654, 2);
        host_.InvokeForAnalysis(0x576740, host_.InteriorForAnalysis(0, 0x765abc), {4});
        return true;
    }
    bool wxAppHelper::RunStageForAnalysis(std::uint32_t stage)
    {
        switch (stage)
        {
        case 0:
            PrepareSceneStorage(); runtime_.field7C = 0xffffffff;
            Call(0x408000, 0); Call(0x4e4150, 0);
            {
                auto scene = host_.ReadHandleForAnalysis(host_.ResolveForAnalysis(0x75db8c, false), 0x18);
                host_.InvokeForAnalysis(0x593860, Call(0x40dff0, 0), {scene});
            }
            host_.InvokeForAnalysis(0x4e28c0, Call(0x4e4090, 0)); Service(0x765bcc); break;
        case 1: host_.InvokeForAnalysis(0x5a0f20, Call(0x4ef6c0, 0)); break;
        case 2: Call(0x40e050, 0); Call(0x4db290, 0); break;
        case 3:
            host_.InvokeForAnalysis(0x5539d0, Call(0x4e40d0, 0));
            host_.InvokeVirtualForAnalysis(0x1c, Call(0x4e4170, 0));
            host_.InvokeForAnalysis(0x4ec030, Call(0x4e4130, 0));
            host_.InvokeForAnalysis(0x4f0360, Call(0x512150, 0)); break;
        case 4: Call(0x55c470, 0x755284); break;
        case 5:
        {
            Call(0x4e40f0, 0);
            auto flag = GetConfigurationForAnalysis().showCinematics;
            host_.WriteByteForAnalysis(Call(0x4e40f0, 0), 0x2c, flag);
            Call(0x5845f0, 0); break;
        }
        case 6:
        {
            constexpr std::uint32_t ids[] = {0x37810000,0x551015c7,0x680838e4,0x1bc6108e,
                0x28c56217,0x17af67d1,0x7fc114c3,0x0982734a};
            constexpr std::uint32_t operations[] = {0x80000044,0x506c70,0x80000038,0x4fcb70,
                0x4fe440,0x80000038,0x4db560,0x587ee0};
            for (std::size_t i = 0; i < 8; ++i)
            {
                auto root = host_.ReadHandleForAnalysis(Service(0x765ad4), 0x2b4);
                auto node = host_.InvokeForAnalysis(0x599120, 0, {root, ids[i]});
                if (operations[i] & 0x80000000) host_.InvokeVirtualForAnalysis(operations[i] & 0xff, node);
                else host_.InvokeForAnalysis(operations[i], node);
                if (i == 4) host_.InvokeForAnalysis(0x4fd8a0, node);
            }
            break;
        }
        case 7:
        {
            auto root = host_.ReadHandleForAnalysis(Service(0x765ad4), 0x2b4);
            auto node = host_.InvokeForAnalysis(0x599120, 0, {root,0xccabefea});
            host_.InvokeForAnalysis(0x586fd0, node); break;
        }
        case 8: Call(0x417c50, 0x75ac60); GameSlot(0xec, 0x34); break;
        case 9: GameSlot(0x120, 0x34); break;
        case 10: GameSlot(0x11c, 0x34); break;
        case 11: GameSlot(0x114, 0x34); break;
        case 12: GameSlot(0x10c, 0x34); break;
        case 13: GameSlot(0x118, 0x34); break;
        case 15: GameSlot(0x12c, 0x34); break;
        case 16:
            GameSlot(0x110, 0x34); Current(); Current();
            Current().ClosePackageForAnalysis("Init"); Call(0x417c50, 0x75ac60);
            Call(0x573a00, 0); Call(0x4b9100, 0, {0}); break;
        case 17: OpenPackageForAnalysis("MainMenu", 1); GameSlot(0xf4, 0x34); break;
        case 18: GameSlot(0xf8, 0x34); break;
        case 19: GameSlot(0xfc, 0x34); break;
        case 20: GameSlot(0x104, 0x34); break;
        case 21: runtime_.menuReady = true; ClosePackageForAnalysis("MainMenu"); break;
        default: break;
        }
        return true;
    }
    void wxAppHelper::InitializeMenuForAnalysis()
    {
        for (std::uint32_t i = 17; i < 22; ++i) RunStageForAnalysis(i);
        runtime_.menuReady = true;
    }
    bool wxAppHelper::UpdateForAnalysis()
    {
        auto simple = [&] {
            Call(0x596410, 0x755294); Call(0x4e2c40, 0x7552a0);
            host_.InvokeVirtualForAnalysis(0x1c, Service(0x755298));
        };
        if (!runtime_.active) { simple(); return true; }
        if (runtime_.countdown)
        {
            simple();
            if (host_.ReadByteForAnalysis(Service(0x75528c), 0x345)) --runtime_.countdown;
            return true;
        }
        Call(0x41e450, 0x755278, {1}); Call(0x574ae0, 0x765acc);
        if (Current().GetConfigurationForAnalysis().buildPCK) BuildPackagesStepForAnalysis();
        host_.InvokeVirtualForAnalysis(0x1c, Service(0x765bf8));
        Call(0x596410, 0x755294);
        auto delta = host_.ReadWordForAnalysis(Service(0x755274), 0x7c);
        Call(0x577b00, 0x765ae8, {delta}); Call(0x4e2c40, 0x7552a0); Call(0x55c460, 0x755284);
        host_.InvokeVirtualForAnalysis(0x1c, Service(0x755298));
        Call(0x5a5cf0, 0x765bc0); Call(0x4f3df0, 0x765af8);
        host_.InvokeVirtualForAnalysis(0x1c, Service(0x765af0));
        host_.InvokeVirtualForAnalysis(0x24, Service(0x765af4));
        host_.InvokeVirtualForAnalysis(0x1c, Service(0x765ae4));
        host_.InvokeVirtualForAnalysis(0x1c, Service(0x765ae0));
        Call(0x4f1220, 0x765b00); Call(0x4e6680, 0x765ad4); Call(0x5944b0, 0x75528c);
        host_.InvokeForAnalysis(0x413b40, host_.InteriorForAnalysis(Service(0x755298), 0x44));
        return true;
    }
    void wxAppHelper::ShutdownForAnalysis()
    {
        // The native sequence may delete this singleton before finishing. Cache
        // the external host reference and never access members after that point.
        auto& host = host_;
        host.InvokeForAnalysis(0x5975b0, reinterpret_cast<Handle>(this));
        if (!Current().GetConfigurationForAnalysis().buildPCK)
            for (auto name : {"LOADSCRN", "BLOOM", "INGAME", "SUBTITLE"}) ClosePackageForAnalysis(name);
        auto owner = host.ReadHandleForAnalysis(Service(0x755274), 0x18);
        host.InvokeForAnalysis(0x40fb60, owner, {reinterpret_cast<Handle>(this)});
        constexpr std::uint32_t services[] = {0x765ae8,0x755294,0x765ad4,0x755294,0x755280,
            0x7552a0,0x755284,0x755298,0x765ad8,0x765bc0,0x765c04,0x765af8,0x765af0,
            0x765bcc,0x765adc,0x75528c,0x75527c,0x765c08,0x755278,0x755290,
            0x765ae0,0x765aec,0x765af4,0x765b00,0x765ae4,0x765bf4,0x765acc,
            0x75529c,0x765b68,0x765bf8};
        for (auto id : services)
        {
            if (auto object = host.ResolveForAnalysis(id, false)) host.InvokeVirtualForAnalysis(0, object, {1});
            host.ClearServiceForAnalysis(id);
        }
    }

    void wxAppHelper::BuildPackagesStepForAnalysis()
    {
        // Original function statics survive destruction/recreation of wxAppHelper.
        struct BuildState
        {
            bool initialized = false;
            std::uint32_t index = 0, variant = 0, first = 0, last = 0, countdown = 0;
            std::array<std::uint32_t, 81> memory{};
        };
        static BuildState state;
        if (!state.initialized)
        { state.initialized = true; state.index = Current().GetConfigurationForAnalysis().firstPCKLevel - 1; }
        if (state.index == Current().GetConfigurationForAnalysis().firstPCKLevel - 1) state.memory.fill(0);
        if (!Current().runtime_.active || state.index == 81) return;
        if (state.countdown)
        {
            if (state.index == 3 || state.index == 13 || state.index == 8 || state.index == 36 || state.index == 37 || state.index == 16)
            {
                if (state.countdown == 5) host_.InvokeVirtualForAnalysis(0x20, Service(0x765af8));
                else if (state.countdown == 15) host_.InvokeVirtualForAnalysis(0x24, Call(0x4f4e90, 0));
            }
            --state.countdown; return;
        }
        auto range = [&](std::uint32_t level) {
            if (level <= 29) { state.first = 2; state.last = 7; }
            else if (level <= 32) state.first = state.last = 5;
            else { state.first = 8; state.last = 9; }
        };
        Handle game = 0;
        for (;;)
        {
            game = Service(0x755294);
            auto level = host_.ReadWordForAnalysis(game, 0x1b0);
            if (level >= 27 && level <= 35)
            {
                range(level);
                if (static_cast<std::int32_t>(state.variant) < static_cast<std::int32_t>(state.last))
                { ++state.variant; break; }
            }
            ++state.index;
            if (state.index == 81)
            {
                auto current = static_cast<std::int32_t>(host_.ReadWordForAnalysis(game, 0x1ac));
                if (current > -1 && host_.ReadHandleForAnalysis(game, 0x15c + 4 * current))
                { Call(0x48eaa0, 0x755294); Call(0x596140, 0x755294); }
                Current().CheckOneLinersForAnalysis(); Current().BuildInGamePackageForAnalysis();
                Call(0x5598e0, 0x755284); Call(0x4e6020, 0x765ad4);
                host_.InvokeTextForAnalysis(0x4135e0, 0, "PCK build finished\n");
                return;
            }
            if (state.index >= 81) continue;
            auto node = host_.ReadHandleForAnalysis(game, 0x18 + state.index * 4);
            if (!node || !host_.ReadByteForAnalysis(node, 0x14)) continue;
            if (state.index >= 27 && state.index <= 35) range(state.index);
            else state.first = state.last = 1;
            state.variant = state.first; break;
        }
        host_.WriteWordForAnalysis(Service(0x765ad4), 0x514, state.variant);
        game = Service(0x755294);
        auto current = static_cast<std::int32_t>(host_.ReadWordForAnalysis(game, 0x1ac));
        if (current > -1 && host_.ReadHandleForAnalysis(game, 0x15c + 4 * current))
        {
            Call(0x417c50, 0x75ac60);
            std::uint32_t bytes = 0;
            for (std::uint32_t i = 0;; ++i)
            {
                auto pool = Service(0x75ac60);
                if (i >= host_.ReadWordForAnalysis(pool, 0x2c)) break;
                bytes += host_.ReadWordForAnalysis(host_.ReadHandleForAnalysis(pool, 0x14 + i * 4), 0x30);
            }
            if (bytes > state.memory[state.index]) state.memory[state.index] = bytes;
            for (std::uint32_t i = 0; i < 81; ++i) if (state.memory[i])
                host_.ReportStateMemoryForAnalysis(static_cast<std::int32_t>(i) - 1, state.memory[i] / 1048576.0);
            Call(0x595450, 0x755294, {state.index,0});
        }
        else Call(0x5953e0, 0x755294, {state.index,0});
        state.countdown = 20;
    }
}
