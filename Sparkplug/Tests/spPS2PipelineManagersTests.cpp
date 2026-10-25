#include "Code/SparkplugPS2/spPS2MicrocodeManager.h"
#include "Code/SparkplugPS2/spPS2PipelineManager.h"

#include <algorithm>
#include <array>
#include <cstdint>
#include <cstdio>
#include <limits>
#include <stdexcept>
#include <string>
#include <vector>

namespace
{
    using namespace sparkplug::reconstruction;
    unsigned checks = 0;
    void Require(bool condition, const char* message)
    {
        ++checks;
        if (!condition) throw std::runtime_error(message);
    }
    struct FactoryState final
    {
        unsigned calls = 0;
        std::uintptr_t next = 0;
        spPS2PipelineManager* manager = nullptr;
        static std::uintptr_t Create(void* context)
        {
            auto& state = *static_cast<FactoryState*>(context);
            ++state.calls;
            return state.next++;
        }
        static std::uintptr_t ReplaceSelf(void* context)
        {
            auto& state = *static_cast<FactoryState*>(context);
            ++state.calls;
            state.manager->RegisterFactoryForAnalysis(-1, {&Create, context});
            return 77;
        }
    };
    void Registry()
    {
        spPS2PipelineManager manager;
        Require(manager.FactoryCountForAnalysis() == 0, "constructor registry empty");
        FactoryState first{0, 100, &manager}, second{0, 200, nullptr};
        Require(manager.RegisterFactoryForAnalysis(-1, {&FactoryState::Create, &first})
            && manager.RegisterFactoryForAnalysis(0, {&FactoryState::Create, &second})
            && manager.RegisterFactoryForAnalysis(std::numeric_limits<std::int32_t>::min(), {})
            && manager.RegisterFactoryForAnalysis(std::numeric_limits<std::int32_t>::max(), {}),
            "signed key boundaries and null bindings register");
        auto result = manager.CreatePipelineForAnalysis(-1);
        Require(result.completed && result.pipeline == 100 && first.calls == 1,
            "found callback invoked");
        result = manager.CreatePipelineForAnalysis(-1);
        Require(result.completed && result.pipeline == 101 && first.calls == 2,
            "pipeline instances not cached");
        manager.RegisterFactoryForAnalysis(-1, {&FactoryState::Create, &second});
        Require(manager.FactoryCountForAnalysis() == 4, "duplicate replaces without extra node");
        result = manager.CreatePipelineForAnalysis(-1);
        Require(result.completed && result.pipeline == 200 && first.calls == 2,
            "replacement binding used");
        Require(!manager.CreatePipelineForAnalysis(7).completed
            && !manager.CreatePipelineForAnalysis(std::numeric_limits<std::int32_t>::min()).completed,
            "native undefined miss/null callbacks remain incomplete");
        spPS2PipelineManager::FactoryBindingForAnalysis untouched{&FactoryState::Create, &first};
        Require(!manager.FindFactoryForAnalysis(7, untouched) && untouched.context == &first,
            "lookup miss leaves output untouched");
        first.next = 0;
        manager.RegisterFactoryForAnalysis(-1, {&FactoryState::ReplaceSelf, &first});
        Require(manager.CreatePipelineForAnalysis(-1).pipeline == 77
            && manager.CreatePipelineForAnalysis(-1).pipeline == 0,
            "callback may replace itself; valid callback null result remains completed");
        Require(manager.CreatePipelineForAnalysis(-1).completed,
            "callback result zero is distinct from missing callback");
        auto clone = manager.Clone();
        const auto* cloned = dynamic_cast<spPS2PipelineManager*>(clone.get());
        Require(cloned && cloned->FactoryCountForAnalysis() == 0,
            "clone constructs fresh registry via inherited base copy");
    }
    void Selection()
    {
        const std::array<std::uint32_t, 8> table{{0, 0, 0, 0x0043A600, 0, 0, 0x0043CF20, 0x0043FFE0}};
        auto selected = SelectPS2Key0PipelineForAnalysis({3, 3, 0}, table.data(), table.size());
        Require(selected.tableIndexInRange && !selected.missingProgramRequiresDiagnostic
            && selected.capacityDefined && selected.programAddress == 0x0043A600
            && selected.field0C == 380, "slot3 and no-field3C capacity");
        selected = SelectPS2Key0PipelineForAnalysis({6, 3, 1}, table.data(), table.size());
        Require(selected.programAddress == 0x0043CF20 && selected.field0C == 254,
            "slot6 and nonzero-field3C capacity");
        selected = SelectPS2Key0PipelineForAnalysis({7, 4, 0xFFFFFFFF}, table.data(), table.size());
        Require(selected.programAddress == 0x0043FFE0 && selected.field0C == 250,
            "field3C is a boolean condition, not a multiplier");
        selected = SelectPS2Key0PipelineForAnalysis({0, 3, 0}, table.data(), table.size());
        Require(selected.tableIndexInRange && selected.missingProgramRequiresDiagnostic
            && selected.capacityDefined && selected.field0C == 380,
            "zero slot diagnostics branch resumes capacity if diagnostics returns");
        Require(!SelectPS2Key0PipelineForAnalysis({8, 3, 0}, table.data(), table.size()).tableIndexInRange
            && !SelectPS2Key0PipelineForAnalysis({0, 0, 0}, nullptr, 8).tableIndexInRange,
            "host table bounds are explicit");
        selected = SelectPS2Key0PipelineForAnalysis({3, 0xFFFFFFFA, 0}, table.data(), table.size());
        Require(selected.tableIndexInRange && !selected.capacityDefined,
            "wrapped divisor zero does not invoke host division");
        selected = SelectPS2Key0PipelineForAnalysis({3, 0xFFFFFFFB, 0}, table.data(), table.size());
        Require(selected.capacityDefined && selected.field0C == 382,
            "nonzero wrapped divisor and low32 product semantics");
    }
    void TagEquals(const std::vector<std::uint32_t>& words, std::size_t at,
        std::array<std::uint32_t, 4> expected, const char* message)
    {
        Require(std::equal(expected.begin(), expected.end(), words.begin() + at), message);
    }
    void Commands()
    {
        spPS2MicrocodeManager manager;
        std::vector<std::uint32_t> words(32, 0xDEADBEEF);
        PS2PipelineCommandBufferForAnalysis buffer{words.data(), words.size(), 0x1000, 0x1020, 0x1000};
        Require(manager.CurrentPacketForAnalysis() == 0, "new microcode cache zero");
        Require(manager.AppendPipelineCommandsForAnalysis(0x8043A600, buffer), "changed upload accepted");
        TagEquals(words, 0, {0x50000000, 0x0043A600, 0, 0}, "CALL clears address bit31 only in tag");
        TagEquals(words, 8, {0x10000000, 0, 0, 0x14000000}, "new CNT QWC zero and MSCAL0");
        TagEquals(words, 12, {0x70000000, 0, 0, 0}, "END appended");
        Require(words[4] == 0xDEADBEEF && buffer.tailAddress == 0x1030
            && buffer.writeAddress == 0x1040 && manager.CurrentPacketForAnalysis() == 0x8043A600,
            "untouched payload, new cursor/tail, full32 packet cache");
        Require(manager.AppendPipelineCommandsForAnalysis(0x8043A600, buffer), "cache hit accepted");
        TagEquals(words, 0, {0x50000000, 0x0043A600, 0, 0}, "cache hit preserves prior CALL");
        TagEquals(words, 12, {0x10000000, 0, 0, 0x14000000}, "cache hit still writes CNT and MSCAL0");
        TagEquals(words, 16, {0x70000000, 0, 0, 0}, "cache hit still appends END");
        Require(buffer.tailAddress == 0x1040 && buffer.writeAddress == 0x1050,
            "cache hit advances one qword");
        Require(manager.AppendPipelineCommandsForAnalysis(0x0043A600, buffer),
            "same masked address with different raw bit31 is a cache miss");
        TagEquals(words, 16, {0x50000000, 0x0043A600, 0, 0}, "raw32 compare is separate from CALL mask");
    }
    void CountAndGuards()
    {
        spPS2MicrocodeManager manager;
        std::vector<std::uint32_t> words(28, 0xA5A5A5A5);
        PS2PipelineCommandBufferForAnalysis buffer{words.data(), words.size(), 0x2000, 0x2040, 0x2000};
        Require(manager.AppendPipelineCommandsForAnalysis(0, buffer), "initial zero packet skips upload but dispatches");
        TagEquals(words, 0, {0x10000003, 0, 0, 0x14000000}, "QWC counts three existing payload qwords");
        Require(words[4] == 0xA5A5A5A5 && words[15] == 0xA5A5A5A5,
            "payload between patched tail and END unchanged");
        const auto before = words;
        const auto tail = buffer.tailAddress, cursor = buffer.writeAddress;
        buffer.wordCount = 22; // Changed upload requires two new qwords, only one fits.
        Require(!manager.AppendPipelineCommandsForAnalysis(1, buffer)
            && words == before && manager.CurrentPacketForAnalysis() == 0
            && buffer.tailAddress == tail && buffer.writeAddress == cursor,
            "short changed-upload span fails before memory/cache/cursor changes");
        buffer.wordCount = words.size(); buffer.tailAddress += 4;
        Require(!manager.AppendPipelineCommandsForAnalysis(0, buffer), "unaligned tail rejected");
        buffer.tailAddress = buffer.writeAddress;
        Require(!manager.AppendPipelineCommandsForAnalysis(0, buffer), "nonpreceding tail rejected");
        buffer = {words.data(), words.size(), 0xFFFFFFD0, 0xFFFFFFF0, 0xFFFFFFD0};
        Require(!manager.AppendPipelineCommandsForAnalysis(0, buffer), "cursor wrap is outside host contract");
        std::vector<std::uint32_t> longWords(4U * 65539U, 0xCCCCCCCC);
        buffer = {longWords.data(), longWords.size(), 0x1000, 0x101010, 0x1000};
        Require(manager.AppendPipelineCommandsForAnalysis(0, buffer), "bounded long CNT packet accepted");
        TagEquals(longWords, 0, {0x10000000, 0, 0, 0x14000000}, "native SH truncates QWC65536 to zero");
    }
    void FactoryCloneAndSingleton()
    {
        (void)spPS2PipelineManager::StaticRTTI();
        (void)spPS2MicrocodeManager::StaticRTTI();
        auto pipeline = spRTTIManager::Instance().Create(spPS2PipelineManager::ClassID);
        auto micro = spRTTIManager::Instance().Create(spPS2MicrocodeManager::ClassID);
        Require(dynamic_cast<spPS2PipelineManager*>(pipeline.get())
            && dynamic_cast<spPS2MicrocodeManager*>(micro.get()), "registered factories create correct types");
        auto& manager = *static_cast<spPS2MicrocodeManager*>(micro.get());
        manager.SetName("micro-name");
        std::array<std::uint32_t, 16> words{};
        PS2PipelineCommandBufferForAnalysis buffer{words.data(), words.size(), 0x1000, 0x1010, 0x1000};
        Require(manager.AppendPipelineCommandsForAnalysis(0x443110, buffer), "prime original cache before clone");
        auto clone = manager.Clone();
        auto* copied = dynamic_cast<spPS2MicrocodeManager*>(clone.get());
        Require(copied && std::string(copied->GetName()) == "micro-name"
            && copied->CurrentPacketForAnalysis() == 0, "clone copies name but not microcode cache");
        Require(spPS2MicrocodeManager::SingletonForAnalysis() == copied,
            "fresh clone constructor replaces singleton");
        micro.reset();
        Require(spPS2MicrocodeManager::SingletonForAnalysis() == nullptr,
            "destroying older instance clears singleton unconditionally");
        pipeline.reset();
        Require(spPS2PipelineManager::SingletonForAnalysis() == nullptr,
            "pipeline destructor clears singleton");
    }
}
int main()
{
    try
    {
        Registry(); Selection(); Commands(); CountAndGuards(); FactoryCloneAndSingleton();
        std::printf("PASS %u/%u: PS2 pipeline manager operations and packet words\n", checks, checks);
        return 0;
    }
    catch (const std::exception& error)
    {
        std::fprintf(stderr, "%s\n", error.what());
        return 1;
    }
}
