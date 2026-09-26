#include "Code/wxAnimationManager.h"
#include "Code/wxAnimationLoader.h"
#include "Analysis/Host/wxAnimationLoaderManagerHost.h"
#include "Analysis/PC/wxAnimationManagerAbi.h"
#include "Analysis/PS2/wxAnimationManagerAbi.h"
#include <cstdlib>
#include <iostream>
#include <sstream>
#include <vector>

namespace
{
    using namespace winx::reconstruction;
    using namespace sparkplug::reconstruction;
    using Manager = wxAnimationManager;
    void Check(bool ok, const char* message) { if (!ok) { std::cerr << message << '\n'; std::exit(1); } }
    std::string Token(std::istream& in) { std::string s; in >> s; return s == "-" ? "" : s; }
    Manager::Row ReadRow(std::istream& in) { Manager::Row row; for (auto& s : row) s = Token(in); return row; }
    struct Host final : wxAnimationManagerHost
    {
        std::uint32_t level = 0, value = 7;
        bool open = true, shared = false;
        std::vector<Manager::Row> rows;
        std::size_t cursor = 0;
        std::string trace;
        void Event(const std::string& s) { if (!trace.empty()) trace += ';'; trace += s; }
        std::uint32_t GetCurrentLevelForAnalysis() override { Event("level:" + std::to_string(level)); return level; }
        std::string ResolveAnimationPathForAnalysis(std::string_view name, std::uint32_t category, std::uint32_t group, std::size_t capacity) override
        {
            Check(capacity == 0x136, "path capacity");
            Event("path:" + std::string(name) + ':' + std::to_string(category) + ':' + std::to_string(group));
            return std::string(name) + (shared ? "" : "/" + std::to_string(category) + "/" + std::to_string(group));
        }
        void* LoadAnimationForAnalysis(std::string_view path) override
        { Event("load:" + std::string(path)); return reinterpret_cast<void*>(std::uintptr_t(value)); }
        void DestroyAnimationForAnalysis(void* p) noexcept override { Event("destroy:" + std::to_string(std::uintptr_t(p))); }
        void* OpenTableForAnalysis(std::string_view name, std::uint32_t category, std::uint32_t group, std::uint32_t flags) override
        {
            Check(category == 10 && flags == 0, "table open arguments");
            Event("open:" + std::string(name) + ':' + std::to_string(group)); cursor = 0;
            return open ? reinterpret_cast<void*>(1) : nullptr;
        }
        void* CreateTokenStreamForAnalysis(void* source) override
        { Check(source == reinterpret_cast<void*>(1), "source stream"); Event("prepare"); return reinterpret_cast<void*>(2); }
        std::string ReadTokenForAnalysis(void* parser, std::size_t capacity, char delimiter) override
        {
            constexpr unsigned sizes[]{20,20,40,20,10,20,5,20};
            const auto field = cursor % 8, row = cursor++ / 8;
            Check(parser == reinterpret_cast<void*>(2) && capacity == sizes[field] && delimiter == (field == 7 ? ';' : ','), "token arguments/order");
            return row < rows.size() ? rows[row][field] : field == 0 ? "end" : "";
        }
        void CloseAndDestroyStreamForAnalysis(void* stream) noexcept override
        { Event("close:" + std::to_string(std::uintptr_t(stream))); }
    };
    std::string Run(const std::string& input)
    {
        Host h; std::string state;
        {
            Manager manager(h); std::istringstream in(input); char op; std::uint32_t a,b;
            while (in >> op)
            {
                switch (op)
                {
                case 'b': h.rows.push_back(ReadRow(in)); break;
                case 'e': h.rows.clear(); break;
                case 'g': in >> h.level; break;
                case 'o': in >> h.open; break;
                case 'v': in >> h.value; break;
                case 'p': in >> h.shared; break;
                case 'l': in >> a; manager.LoadSetForAnalysis(a); break;
                case 'd': in >> a; manager.ReleaseSetForAnalysis(a); break;
                case 'a': { auto name = Token(in); in >> a; auto* r = manager.GetAnimationForAnalysis(name,a); h.Event("result:" + std::to_string(std::uintptr_t(r))); break; }
                case 'q': in >> a >> b; h.Event("result:" + std::to_string(std::uintptr_t(manager.SelectForAnalysis(a,b)))); break;
                case 't': { in >> a; auto token = Token(in); h.Event("result:" + std::to_string(Manager::ParseTokenForAnalysis(static_cast<Manager::TokenForAnalysis>(a),token))); break; }
                case 'x': manager.ClearForAnalysis(); break;
                default: Check(false,"protocol operation");
                }
                Check(!in.fail(), "protocol arguments");
            }
            std::ostringstream out;
            for (const auto& entry : manager.GetCacheForAnalysis())
                out << entry.first << ':' << std::uintptr_t(entry.second.resource) << ':' << entry.second.owner << ',';
            out << '|';
            for (std::size_t i=0;i<Manager::TableCount;++i)
                for (const auto& entry : manager.GetTablesForAnalysis()[i]) out << i << ':' << entry.first << ':' << std::uintptr_t(entry.second) << ',';
            state = out.str();
        }
        return state + '|' + h.trace;
    }
    struct LoaderHost final : wxAnimationLoaderManagerHost
    {
        Manager& manager; unsigned destroyed = 0;
        explicit LoaderHost(Manager& m) : manager(m) {}
        Manager& ResolveAnimationManagerForAnalysis() noexcept override { return manager; }
        void ConstructEntityForAnalysis(wxAnimationLoader&, bool enabled) override { Check(enabled,"entity enabled"); }
        void DestroyEntityForAnalysis(wxAnimationLoader&) noexcept override { ++destroyed; }
        bool CopyEntityForAnalysis(const wxAnimationLoader&,wxAnimationLoader&,spCloneManager&) const override { return true; }
        std::uint32_t GetCurrentLevelForAnalysis() noexcept override { return 37; }
    };
}
int main(int argc, char** argv)
{
    if (argc == 2 && std::string(argv[1]) == "--protocol")
    { std::string s; while (std::getline(std::cin,s)) std::cout << Run(s) << '\n'; return 0; }
    Check(Manager::ParseTokenForAnalysis(Manager::TokenForAnalysis::Action,"convaincedTalking") == 36, "original spelling");
    Check(Manager::ParseTokenForAnalysis(Manager::TokenForAnalysis::Action,"convincedTalking") == 0, "unknown defaults zero");
    Check(Run("p 1 a shared 1 a shared 2 d 2 a shared 2 d 1").find("||path:shared:2:2;load:shared;result:7;path:shared:2:5;result:7;path:shared:2:5;result:7;destroy:7") == 0,
        "first cache owner survives other set release");
    Check(Run("v 0 a absent 0 v 7 a absent 0").find("absent/2/1:0:0,") == 0,"null cached without retry");
    Host h;
    {
        Manager manager(h);
        h.rows.push_back({"moving","none","none","none","none","main","0","idle"});
        manager.LoadSetForAnalysis(0);
        Check(manager.SelectForAnalysis(0,0) == reinterpret_cast<void*>(7), "load-to-select");
        spCloneManager clones;
        auto copy = clones.Clone(manager);
        auto& clone = static_cast<Manager&>(*copy);
        Check(clone.GetCacheForAnalysis().empty() && Manager::GetInstance() == &clone, "clone empty and replaces singleton");
        copy.reset(); Check(!Manager::GetInstance(), "destructor unconditionally clears singleton");
        manager.ClearForAnalysis();
        bool missing = false;
        try { (void)manager.SelectForAnalysis(8,0); } catch (const std::logic_error&) { missing = true; }
        Check(missing,"missing native default has explicit portable guard");
        LoaderHost loaderHost(manager);
        h.trace.clear();
        {
            wxAnimationLoader loader(loaderHost);
            std::uint32_t message = 0x1D; loader.vfunc_0C(&message);
            Check(manager.GetCacheForAnalysis().size()==2 && manager.SelectForAnalysis(0,1),"loader/manager/resource integration");
        }
        Check(manager.GetCacheForAnalysis().empty() && manager.GetTablesForAnalysis()[1].empty() && loaderHost.destroyed==1,
            "loader teardown releases both resources before entity destruction");
        Manager::SetFactoryHostForAnalysis(&h);
        auto made=spRTTIManager::Instance().Create(Manager::ClassID);
        Check(made && made->IsExactly(Manager::ClassID), "RTTI factory");
        Manager::SetFactoryHostForAnalysis(nullptr);
    }
    Check(!Manager::GetInstance(), "final singleton cleared");
    std::cout << "wxAnimationManager checks passed\n";
}
