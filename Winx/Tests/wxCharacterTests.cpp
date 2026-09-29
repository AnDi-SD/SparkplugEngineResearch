#include "Code/wxCharacter.h"
#include "Analysis/PC/wxCharacterAbi.h"
#include "Analysis/PS2/wxCharacterAbi.h"
#include <cstdlib>
#include <cstring>
#include <iostream>
#include <string>
#include <vector>

using namespace winx::reconstruction;
using namespace sparkplug::reconstruction;

namespace
{
    void Check(bool condition, const char* message)
    {
        if (!condition) { std::cerr << message << '\n'; std::exit(1); }
    }
    struct Host final : wxCharacterHost
    {
        mutable int copies = 0;
        bool copyOkay = true;
        std::vector<std::string> calls;
        void ConstructEntityForAnalysis(wxCharacter&) override { calls.emplace_back("construct"); }
        void DestroyEntityForAnalysis(wxCharacter&) noexcept override { calls.emplace_back("destroy"); }
        bool CopyEntityForAnalysis(const wxCharacter&, wxCharacter&,
            spCloneManager&) const override
        {
            ++copies;
            return copyOkay;
        }
        void RegisterCharacterForAnalysis(wxCharacter&, std::uint32_t group) override
        { calls.emplace_back("register" + std::to_string(group)); }
        void UnregisterCharacterForAnalysis(wxCharacter&, std::uint32_t group) noexcept override
        { calls.emplace_back("unregister" + std::to_string(group)); }
        void ReleaseReferenceForAnalysis(wxCharacter&, std::uint32_t offset,
            std::uint32_t value) noexcept override
        { calls.emplace_back("release" + std::to_string(offset) + ":" + std::to_string(value)); }
        void DetachCharacterForAnalysis(wxCharacter&) noexcept override
        { calls.emplace_back("detach"); }
        void AssignEntityReferenceForAnalysis(wxCharacter&, void*) override
        { calls.emplace_back("assign"); }
        void ClearExternalFlagForAnalysis(wxCharacter&, std::uint32_t mask) override
        { calls.emplace_back("clear" + std::to_string(mask)); }
    };
    void PutWord(wxCharacter& character, std::size_t offset, std::uint32_t word)
    {
        std::memcpy(character.GetOwnBytesForAnalysis().data() + offset - 0x124,
            &word, sizeof(word));
    }
}

int main()
{
    static_assert(sizeof(winx::evidence::pc::wxCharacterLayout) == 0x160);
    static_assert(sizeof(winx::evidence::ps2::wxCharacterLayout) == 0x170);
    static_assert(winx::evidence::pc::wxCharacterFactory == 0x004015E0);
    static_assert(winx::evidence::ps2::wxCharacterFactory == 0x003F77A0);
    Check(wxCharacter::StaticRTTI().baseClassID == 0x796A1869,
        "registration parent");
    Host host;
    wxCharacter::SetFactoryHostForAnalysis(&host);
    {
        wxCharacter source(host), target(host);
        Check(source.IsExactly(wxCharacter::ClassID)
            && source.IsKindOf(0x796A1869), "class identity");
        Check(source.GetOwnBytesForAnalysis()[0x148 - 0x124] == 1,
            "constructor link selector");
        Check(host.calls == std::vector<std::string>{"construct", "register25",
            "register37", "construct", "register25", "register37"},
            "construction and subscriptions");

        for (std::size_t i = 0; i < 0x3C; ++i)
        {
            source.GetOwnBytesForAnalysis()[i] = static_cast<std::uint8_t>(i + 1);
            target.GetOwnBytesForAnalysis()[i] = 0xA5;
        }
        PutWord(source, 0x148, 0);
        const auto callsBeforeAssignment = host.calls.size();
        source.vfunc_28_AssignReferenceForAnalysis(&target);
        Check(host.calls.size() == callsBeforeAssignment + 2
            && host.calls[callsBeforeAssignment] == "assign"
            && host.calls[callsBeforeAssignment + 1] == "clear16",
            "unlinked assignment clears external flag after parent call");
        Check(source.vfunc_2C_FlagsAllowForAnalysis(0x10)
            && !source.vfunc_2C_FlagsAllowForAnalysis(0x08), "unlinked mask");
        PutWord(source, 0x148, 1);
        const auto callsBeforeLinkedAssignment = host.calls.size();
        source.vfunc_28_AssignReferenceForAnalysis(&target);
        Check(host.calls.size() == callsBeforeLinkedAssignment + 1
            && host.calls.back() == "assign", "linked assignment keeps flag");
        Check(source.vfunc_30(), "constant virtual predicate");
        Check(!source.vfunc_2C_FlagsAllowForAnalysis(0x10)
            && !source.vfunc_2C_FlagsAllowForAnalysis(0x02)
            && source.vfunc_2C_FlagsAllowForAnalysis(0x04), "linked mask");

        wxCharacter* result = nullptr;
        PutWord(source, 0x14C, 0x2B);
        source.GetOwnBytesForAnalysis()[0x150 - 0x124] = 1;
        source.Handle2749ForAnalysis(0x2B, result);
        Check(result == &source, "matching notification publishes character");
        result = nullptr;
        source.Handle2749ForAnalysis(0x05, result);
        Check(result == nullptr, "other kind remains untouched");
        source.GetOwnBytesForAnalysis()[0x150 - 0x124] = 0;
        source.Handle2749ForAnalysis(0x2B, result);
        Check(result == nullptr, "disabled character remains unpublished");

        const auto before = target.GetOwnBytesForAnalysis();
        spCloneManager manager;
        host.copyOkay = false;
        Check(!source.vfunc_14(target, manager)
            && target.GetOwnBytesForAnalysis() == before, "failed parent leaves own fields");
        host.copyOkay = true;
        Check(source.vfunc_14(target, manager), "successful parent copy");
        for (std::size_t i = 0; i < 0x3C; ++i)
        {
            const auto offset = i + 0x124;
            const bool copied = (offset >= 0x124 && offset < 0x140)
                || (offset >= 0x148 && offset < 0x150)
                || offset == 0x150 || (offset >= 0x154 && offset < 0x158);
            Check(target.GetOwnBytesForAnalysis()[i]
                == (copied ? source.GetOwnBytesForAnalysis()[i] : before[i]),
                "copy transfers exactly eleven fields");
        }
        Check(host.copies == 2, "parent copy called for both outcomes");
    }
    // A fresh instance isolates destructor order from the opaque copy fixture.
    host.calls.clear();
    {
        wxCharacter character(host);
        PutWord(character, 0x130, 0x1234);
        PutWord(character, 0x154, 0x5678);
    }
    Check(host.calls == std::vector<std::string>{"construct", "register25",
        "register37", "release304:4660", "release340:22136",
        "unregister25", "unregister37", "detach", "destroy"},
        "destructor releases and unregisters in native order");
    wxCharacter::SetFactoryHostForAnalysis(nullptr);
    std::cout << "wxCharacter checks passed\n";
}
