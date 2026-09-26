#include "Code/wxAnimationLoader.h"
#include "Analysis/PC/wxAnimationLoaderAbi.h"
#include "Analysis/PS2/wxAnimationLoaderAbi.h"
#include <cstdlib>
#include <iostream>
#include <sstream>
#include <string>

namespace
{
    using namespace winx::reconstruction;
    using namespace sparkplug::reconstruction;
    void Check(bool condition, const char* message)
    {
        if (!condition) { std::cerr << message << '\n'; std::exit(1); }
    }
    struct Host final : wxAnimationLoaderHost
    {
        std::uint32_t level = 0, nextLevel = 0, manager = 1, nextManager = 1;
        bool gameExists = true, mutate = false, copyResult = true;
        unsigned constructs = 0, destructs = 0;
        std::string trace;
        void Event(const std::string& s) { if (!trace.empty()) trace += ';'; trace += s; }
        void ConstructEntityForAnalysis(wxAnimationLoader&, bool enabled) override
        { Check(enabled, "wxEntity(true)"); ++constructs; Event("construct"); }
        void DestroyEntityForAnalysis(wxAnimationLoader&) noexcept override
        { ++destructs; Event("destroy-base"); }
        bool CopyEntityForAnalysis(const wxAnimationLoader&, wxAnimationLoader&, spCloneManager&) const override
        { const_cast<Host*>(this)->Event("copy-base"); return copyResult; }
        std::uint32_t GetCurrentLevelForAnalysis() noexcept override
        {
            if (!gameExists) { Event("game-create"); gameExists = true; }
            return level;
        }
        void* GetAnimationManagerForAnalysis() noexcept override
        {
            if (!manager) { Event("manager-create"); manager = 1; }
            return reinterpret_cast<void*>(std::uintptr_t(manager));
        }
        void Set(void* handle, std::uint32_t set, bool load)
        {
            Event(std::string(load ? "load:" : "release:") + std::to_string(std::uintptr_t(handle)) + ':' + std::to_string(set));
            if (mutate) { level = nextLevel; manager = nextManager; mutate = false; }
        }
        void LoadSetForAnalysis(void* manager, std::uint32_t set) noexcept override { Set(manager, set, true); }
        void ReleaseSetForAnalysis(void* manager, std::uint32_t set) noexcept override { Set(manager, set, false); }
    };
    std::string Run(const std::string& input)
    {
        Host h;
        {
            wxAnimationLoader loader(h);
            std::istringstream in(input);
            char op;
            std::uint32_t value, flag;
            while (in >> op)
            {
                switch (op)
                {
                case 'l': in >> h.level; break;
                case 'a': in >> flag; loader.ApplyCurrentLevelForAnalysis(static_cast<std::uint8_t>(flag)); break;
                case 's': in >> value >> flag; loader.ApplySetForAnalysis(value, static_cast<std::uint8_t>(flag)); break;
                case 'n': in >> value; loader.vfunc_0C(&value); break;
                case 'g': in >> h.gameExists >> h.manager; break;
                case 'j': in >> h.nextLevel >> h.nextManager; h.mutate = true; break;
                case 'c':
                {
                    in >> h.copyResult;
                    wxAnimationLoader destination(h);
                    spCloneManager cm;
                    h.Event("copy-result:" + std::to_string(loader.vfunc_14(destination, cm)));
                    break;
                }
                default: Check(false, "protocol operation");
                }
                Check(!in.fail(), "protocol arguments");
            }
        }
        Check(h.constructs == h.destructs, "entity lifetimes balanced");
        return h.trace;
    }
}
int main(int argc, char** argv)
{
    if (argc == 2 && std::string(argv[1]) == "--protocol")
    {
        std::string s;
        while (std::getline(std::cin, s)) std::cout << Run(s) << '\n';
        return 0;
    }
    Check(Run("") == "construct;release:1:10;release:1:19;release:1:26;release:1:67;destroy-base",
        "destructor releases even without preceding load");
    Check(Run("l 37 n 29 l 36") == "construct;load:1:1;load:1:34;release:1:40;release:1:31;release:1:1;release:1:10;release:1:19;release:1:26;release:1:67;destroy-base",
        "destructor uses current level, not loaded level");
    Check(Run("l 31 g 0 0 a 1 a 1").find("construct;game-create;manager-create;load:1:66;load:1:66;") == 0,
        "lazy singletons and repeated notifications are not suppressed");
    Check(Run("l 11 a 0").find("release:1:66;release:1:21;release:1:10") != std::string::npos,
        "level 11 extra release order");
    Check(Run("l 37 j 31 2 a 1").find("construct;load:1:1;load:2:34;release:2:66;") == 0,
        "level captured once; manager resolved again after callback");
    Host h;
    {
        wxAnimationLoader source(h);
        spCloneManager cm;
        auto copy = cm.Clone(source);
        Check(copy && copy->IsKindOf(0x796A1869), "clone RTTI ancestry");
        Check(h.trace == "construct;construct;copy-base", "clone does not load resources");
        h.copyResult = false;
        spCloneManager failed;
        Check(!source.vfunc_10(failed), "failed Copy rejects clone");
        Check(h.destructs == 1 && h.trace.find("release:1:67;destroy-base") != std::string::npos,
            "failed clone still performs destructor releases");
        wxAnimationLoader::SetFactoryHostForAnalysis(&h);
        auto made = spRTTIManager::Instance().Create(wxAnimationLoader::ClassID);
        Check(made && made->IsExactly(wxAnimationLoader::ClassID), "RTTI factory");
    }
    wxAnimationLoader::SetFactoryHostForAnalysis(nullptr);
    Check(h.constructs == h.destructs, "clone and factory lifetimes balanced");
    std::cout << "wxAnimationLoader checks passed\n";
}
