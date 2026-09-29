#include "Analysis/PC/wxProjectileAbi.h"
#include "Analysis/PS2/wxProjectileAbi.h"
#include "Code/wxProjectile.h"

#include <cstdlib>
#include <cstring>
#include <iostream>
#include <map>
#include <set>
#include <string>
#include <vector>

using namespace winx::reconstruction;
using namespace sparkplug::reconstruction;

namespace
{
    void Require(bool condition, const char* message)
    { if (!condition) { std::cerr << message << '\n'; std::exit(EXIT_FAILURE); } }
    void PutWord(wxProjectile& object, std::size_t offset, std::uint32_t value)
    { std::memcpy(object.GetBytesForAnalysis().data() + offset, &value, 4); }
    std::uint32_t ReadWord(const wxProjectile& object, std::size_t offset)
    {
        std::uint32_t value;
        std::memcpy(&value, object.GetBytesForAnalysis().data() + offset, 4);
        return value;
    }
    struct Host final : wxProjectileHost
    {
        std::map<std::uint32_t, std::uint16_t> counts;
        std::set<void*> actors;
        std::vector<std::string> calls;
        int created = 0;
        int destroyed = 0;
        std::uint16_t ReferenceCountForAnalysis(std::uint32_t token) override
        { return counts.at(token); }
        void SetReferenceCountForAnalysis(std::uint32_t token,
            std::uint16_t count) override
        { counts[token] = count; calls.emplace_back("ref" + std::to_string(token)); }
        void DeleteReferenceForAnalysis(std::uint32_t token) override
        { calls.emplace_back("delete-ref" + std::to_string(token)); }
        void* CreateFreshActorForAnalysis() override
        {
            void* result = new int(++created);
            actors.insert(result);
            calls.emplace_back("create-actor");
            return result;
        }
        void DestroyActorForAnalysis(void* actor) override
        {
            Require(actors.erase(actor) == 1, "actor is owned once");
            delete static_cast<int*>(actor);
            ++destroyed;
            calls.emplace_back("destroy-actor");
        }
        void DestroyProjectileForAnalysis(wxProjectile& object) noexcept override
        {
            if (object.GetActorForAnalysis()) DestroyActorForAnalysis(object.GetActorForAnalysis());
        }
        void CompleteCopyForAnalysis(const wxProjectile&,
            wxProjectile&, std::uint32_t classID) override
        {
            Require(classID == wxProjectile::ClassID, "copy class ID");
            calls.emplace_back("copy-hook");
        }
    };
}

int main()
{
    static_assert(sizeof(winx::evidence::pc::wxProjectileLayout) == 0xEC);
    static_assert(sizeof(winx::evidence::ps2::wxProjectileLayout) == 0xEC);
    Host host;
    wxProjectile::SetFactoryHostForAnalysis(&host);
    auto sourceHolder = spRTTIManager::Instance().Create(wxProjectile::ClassID);
    auto* source = dynamic_cast<wxProjectile*>(sourceHolder.get());
    Require(source && source->IsKindOf(0x796A1869),
        "factory and native RTTI ancestry");
    Require(ReadWord(*source, 0x38) == 0x3F800000
        && ReadWord(*source, 0xD8) == 0xC4750000,
        "observed constructor defaults");
    source->SetName("test projectile");
    wxProjectile destination(host);
    auto before = destination.GetBytesForAnalysis();
    constexpr std::size_t words[]{0x10,0x28,0x34,0x38,0x40,0x44,
        0x48,0x4C,0x50,0x54,0x2C,0x9C};
    for (std::size_t i = 0; i < 12; ++i)
        PutWord(*source, words[i], static_cast<std::uint32_t>(0x11111111u * (i+1)));
    source->GetBytesForAnalysis()[0x30] = 255;
    source->GetBytesForAnalysis()[0xA0] = 128;
    constexpr std::size_t refs[]{0x14,0x58,0x5C,0xD4};
    for (std::size_t i = 0; i < 4; ++i)
    {
        const auto oldToken = static_cast<std::uint32_t>(10+i);
        const auto newToken = static_cast<std::uint32_t>(20+i);
        PutWord(destination, refs[i], oldToken);
        PutWord(*source, refs[i], newToken);
        host.counts[oldToken] = 3;
        host.counts[newToken] = 3;
    }
    spCloneManager manager;
    Require(source->vfunc_14(destination, manager), "first Copy");
    Require(std::string(destination.GetName()) == "test projectile",
        "inherited named Copy");
    for (std::size_t i = 0; i < before.size(); ++i)
    {
        bool copied = i == 0x30 || i == 0xA0;
        for (const auto offset : words)
            if (i >= offset && i < offset+4) copied = true;
        for (const auto offset : refs)
            if (i >= offset && i < offset+4) copied = true;
        Require(destination.GetBytesForAnalysis()[i] ==
            (copied ? source->GetBytesForAnalysis()[i] : before[i]),
            "only measured fields copied");
    }
    for (std::uint32_t i = 0; i < 4; ++i)
        Require(host.counts[10+i] == 2 && host.counts[20+i] == 4,
            "release then retain reference counts");
    Require(host.calls[0] == "ref10" && host.calls[1] == "ref20"
        && host.calls.back() == "copy-hook", "reference and actor order");
    Require(destination.GetActorForAnalysis() && host.created == 1,
        "fresh actor created");

    host.calls.clear();
    Require(source->vfunc_14(destination, manager), "second Copy");
    Require(host.destroyed == 1 && host.created == 2
        && destination.GetActorForAnalysis(),
        "existing target actor replaced with fresh actor");
    Require(host.calls[host.calls.size()-3] == "destroy-actor"
        && host.calls[host.calls.size()-2] == "create-actor"
        && host.calls.back() == "copy-hook", "actor replacement order");
    for (std::uint32_t i = 0; i < 4; ++i)
        Require(host.counts[20+i] == 4,
            "same reference decremented then incremented");

    auto clone = source->vfunc_10(manager);
    auto* cloned = dynamic_cast<wxProjectile*>(clone.get());
    Require(cloned && cloned->GetActorForAnalysis()
        && cloned->GetActorForAnalysis() != destination.GetActorForAnalysis(),
        "clone allocates its own default Actor");
    clone.reset();
    wxProjectile::SetFactoryHostForAnalysis(nullptr);
    std::cout << "wxProjectile checks passed\n";
}
