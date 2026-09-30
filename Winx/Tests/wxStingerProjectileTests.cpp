#include "Analysis/PC/wxStingerProjectileAbi.h"
#include "Analysis/PS2/wxStingerProjectileAbi.h"
#include "Code/wxStingerProjectile.h"

#include <cstdlib>
#include <cstring>
#include <iostream>
#include <map>
#include <string>
#include <vector>

using namespace winx::reconstruction;
using namespace sparkplug::reconstruction;

namespace
{
    void Require(bool condition, const char* message)
    { if (!condition) { std::cerr << message << '\n'; std::exit(EXIT_FAILURE); } }
    void PutWord(wxStingerProjectile& object, std::size_t offset, std::uint32_t value)
    { std::memcpy(object.GetTailForAnalysis().data()+offset-0xEC, &value, 4); }
    std::uint32_t Word(const wxStingerProjectile& object, std::size_t offset)
    {
        std::uint32_t value;
        std::memcpy(&value, object.GetTailForAnalysis().data()+offset-0xEC, 4);
        return value;
    }
    struct Host final : wxProjectileHost
    {
        std::map<std::uint32_t, std::uint16_t> counts;
        std::vector<std::string> calls;
        std::uint16_t ReferenceCountForAnalysis(std::uint32_t token) override
        { return counts.at(token); }
        void SetReferenceCountForAnalysis(std::uint32_t token,
            std::uint16_t count) override
        { counts[token] = count; calls.emplace_back("ref" + std::to_string(token)); }
        void DeleteReferenceForAnalysis(std::uint32_t token) override
        { calls.emplace_back("delete" + std::to_string(token)); }
        void* CreateFreshActorForAnalysis() override { return nullptr; }
        void DestroyActorForAnalysis(void*) override {}
        void DestroyProjectileForAnalysis(wxProjectile&) noexcept override
        { calls.emplace_back("base-destroy"); }
        void CompleteCopyForAnalysis(const wxProjectile&,
            wxProjectile&, std::uint32_t classID) override
        {
            Require(classID == wxStingerProjectile::ClassID,
                "leaf copy class ID");
            calls.emplace_back("copy-hook");
        }
    };
}

int main()
{
    static_assert(sizeof(winx::evidence::pc::wxStingerProjectileLayout) == 0xF8);
    static_assert(sizeof(winx::evidence::ps2::wxStingerProjectileLayout) == 0xF8);
    Host host;
    wxStingerProjectile::SetFactoryHostForAnalysis(&host);
    auto factory = spRTTIManager::Instance().Create(wxStingerProjectile::ClassID);
    auto* source = dynamic_cast<wxStingerProjectile*>(factory.get());
    Require(source && source->IsKindOf(wxProjectile::ClassID)
        && source->IsKindOf(0x796A1869), "factory and native ancestry");
    Require(Word(*source, 0xEC) == 0 && Word(*source, 0xF0) == 0
        && Word(*source, 0xF4) == 0, "PC and PS2 constructor tail");
    source->SetName("stinger");
    wxStingerProjectile destination(host);
    source->GetBytesForAnalysis()[0x28] = 0xA5;
    destination.GetBytesForAnalysis()[0x28] = 0x5A;
    PutWord(*source, 0xEC, 20);
    PutWord(*source, 0xF0, 21);
    PutWord(*source, 0xF4, 0x7FC12345);
    PutWord(destination, 0xEC, 10);
    PutWord(destination, 0xF0, 11);
    for (auto token : {10u,11u,20u,21u}) host.counts[token] = 3;
    spCloneManager manager;
    Require(source->vfunc_14(destination, manager), "leaf Copy");
    Require(std::string(destination.GetName()) == "stinger"
        && destination.GetBytesForAnalysis()[0x28] == 0x5A,
        "named parent Copy without wxProjectile payload Copy");
    Require(Word(destination, 0xEC) == 20 && Word(destination, 0xF0) == 21
        && Word(destination, 0xF4) == 0x7FC12345,
        "two references and one raw word copied");
    Require(host.counts[10] == 2 && host.counts[11] == 2
        && host.counts[20] == 4 && host.counts[21] == 4,
        "reference counts match original order");
    Require(host.calls == std::vector<std::string>{
        "ref10", "ref20", "ref11", "ref21", "copy-hook"},
        "leaf Copy callback order");

    host.calls.clear();
    auto clone = source->vfunc_10(manager);
    auto* copy = dynamic_cast<wxStingerProjectile*>(clone.get());
    Require(copy && Word(*copy, 0xEC) == 20 && Word(*copy, 0xF0) == 21,
        "leaf clone type and references");
    Require(copy->GetBytesForAnalysis()[0x28] == 0,
        "clone leaves base projectile payload at constructor default");
    clone.reset();
    Require(host.calls[host.calls.size()-3] == "ref21"
        && host.calls[host.calls.size()-2] == "ref20"
        && host.calls.back() == "base-destroy",
        "leaf references released in reverse order before base");
    wxStingerProjectile::SetFactoryHostForAnalysis(nullptr);
    std::cout << "wxStingerProjectile checks passed\n";
}
