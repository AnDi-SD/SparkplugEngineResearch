#include "Analysis/PC/wxWebSpitProjectileAbi.h"
#include "Analysis/PS2/wxWebSpitProjectileAbi.h"
#include "Code/wxWebSpitProjectile.h"

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
    void PutWord(wxWebSpitProjectile& object, std::size_t offset, std::uint32_t value)
    { std::memcpy(object.GetTailForAnalysis().data()+offset-0xEC, &value, 4); }
    std::uint32_t Word(const wxWebSpitProjectile& object, std::size_t offset)
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
            Require(classID == wxWebSpitProjectile::ClassID, "leaf copy class ID");
            calls.emplace_back("copy-hook");
        }
    };
}

int main()
{
    static_assert(sizeof(winx::evidence::pc::wxWebSpitProjectileLayout) == 0x100);
    static_assert(sizeof(winx::evidence::ps2::wxWebSpitProjectileLayout) == 0x100);
    Host host;
    wxWebSpitProjectile::SetFactoryHostForAnalysis(&host);
    auto factory = spRTTIManager::Instance().Create(wxWebSpitProjectile::ClassID);
    auto* source = dynamic_cast<wxWebSpitProjectile*>(factory.get());
    Require(source && source->IsKindOf(wxProjectile::ClassID)
        && source->IsKindOf(0x796A1869), "factory and native ancestry");
    Require(source->GetTailForAnalysis()[0] == 0
        && Word(*source, 0xF0) == 0 && Word(*source, 0xF4) == 0
        && Word(*source, 0xF8) == 0 && Word(*source, 0xFC) == 0,
        "PS2 constructor and PC factory defaults");
    source->SetName("web spit");
    wxWebSpitProjectile destination(host);
    source->GetBytesForAnalysis()[0x28] = 0xA5;
    destination.GetBytesForAnalysis()[0x28] = 0x5A;
    source->GetTailForAnalysis()[0] = 255;
    destination.GetTailForAnalysis()[1] = 0xA5;
    PutWord(*source, 0xF0, 0x80000000);
    PutWord(*source, 0xF4, 20);
    PutWord(*source, 0xF8, 21);
    PutWord(*source, 0xFC, 0x7FC12345);
    PutWord(destination, 0xF4, 10);
    PutWord(destination, 0xF8, 11);
    for (auto token : {10u,11u,20u,21u}) host.counts[token] = 3;
    spCloneManager manager;
    Require(source->vfunc_14(destination, manager), "leaf Copy");
    Require(std::string(destination.GetName()) == "web spit"
        && destination.GetBytesForAnalysis()[0x28] == 0x5A,
        "named parent Copy skips common projectile payload");
    Require(destination.GetTailForAnalysis()[0] == 255
        && destination.GetTailForAnalysis()[1] == 0xA5
        && Word(destination, 0xF0) == 0x80000000
        && Word(destination, 0xF4) == 20 && Word(destination, 0xF8) == 21
        && Word(destination, 0xFC) == 0x7FC12345,
        "byte, gaps, words and references");
    Require(host.counts[10] == 2 && host.counts[11] == 2
        && host.counts[20] == 4 && host.counts[21] == 4,
        "reference updates");
    Require(host.calls == std::vector<std::string>{
        "ref10", "ref20", "ref11", "ref21", "copy-hook"},
        "Copy callback order");
    host.calls.clear();
    auto clone = source->vfunc_10(manager);
    auto* copy = dynamic_cast<wxWebSpitProjectile*>(clone.get());
    Require(copy && Word(*copy, 0xF4) == 20 && Word(*copy, 0xF8) == 21,
        "clone keeps leaf type and references");
    Require(copy->GetBytesForAnalysis()[0x28] == 0,
        "clone skips common payload");
    clone.reset();
    Require(host.calls[host.calls.size()-3] == "ref20"
        && host.calls[host.calls.size()-2] == "ref21"
        && host.calls.back() == "base-destroy",
        "destructor releases F4 then F8 before base");
    wxWebSpitProjectile::SetFactoryHostForAnalysis(nullptr);
    std::cout << "wxWebSpitProjectile checks passed\n";
}
