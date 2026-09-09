#include "Code/Sparkplug/spAnimationSerializer.h"
#include "Code/Sparkplug/spNode.h"
#include "Code/Sparkplug/spNodeSerializer.h"
#include "Code/Sparkplug/spSerializerManager.h"
#include "Code/Sparkplug/spResourceFATSerializer.h"
#include "Code/SparkBase/spMemoryStream.h"
#include <cstring>
#include <iomanip>
#include <iostream>
#include <limits>
#include <stdexcept>
#include <vector>

using namespace sparkplug::reconstruction;
namespace
{
    using Bytes = std::vector<std::uint8_t>;
    int checks = 0;
    void Check(bool value, const char* text)
    {
        ++checks;
        if (!value) throw std::runtime_error(text);
    }
    class Output final : public spStream
    {
    public:
        spMemoryStream memory;
        unsigned writes = 0, seeks = 0, failedWrite = 0, failedSeek = 0;
        bool Open(const char* name) override { return memory.Open(name); }
        bool Open(std::uint32_t mode, const char* name) override { return memory.Open(mode, name); }
        bool Close() override { return memory.Close(); }
        bool Seek(SeekSource source, std::int32_t offset) override
        { return ++seeks != failedSeek && memory.Seek(source, offset); }
        bool GetCurrentPosition(std::uint32_t& position) const override { return memory.GetCurrentPosition(position); }
        bool ReadData(void* target, std::uint32_t size) override { return memory.ReadData(target, size); }
        bool WriteData(const void* source, std::uint32_t size) override
        { return ++writes != failedWrite && memory.WriteData(source, size); }
        bool vfunc_WriteFromStream(spStream* source, std::uint32_t size) override { return memory.vfunc_WriteFromStream(source, size); }
        bool GetSize(std::uint32_t* size) const override { return memory.GetSize(size); }
        Bytes Data()
        {
            std::uint32_t size = 0;
            Check(GetSize(&size), "output size");
            const auto* data = static_cast<const std::uint8_t*>(memory.GetBuffer());
            return size ? Bytes(data, data + size) : Bytes{};
        }
    };
    void Register(spSerializerManager& manager)
    {
        manager.SetDispatchContextForAnalysis(1, 2);
        Check(manager.RegisterForAnalysis(spAnimation::ClassID,
              std::make_shared<spAnimationSerializer>(), 0xFF, 3), "register animation serializer");
    }
    std::uint32_t Word(const Bytes& data, std::size_t at)
    {
        Check(at + 4 <= data.size(), "bounded word");
        std::uint32_t value = 0;
        std::memcpy(&value, data.data() + at, 4);
        return value;
    }
    Bytes Success()
    {
        spSerializerManager manager;
        Register(manager);
        spAnimation animation;
        animation.SetName("not an engine NamedObject");
        Output out;
        Check(out.Open(nullptr), "open output");
        std::string error;
        Check(!spSerializer::WriteReferenceForAnalysis(manager, out, &animation, &error) &&
              !error.empty() && out.Data().empty(), "unindexed reference host guard");
        Check(spSerializer::IndexReferenceForAnalysis(manager, nullptr), "null index succeeds");
        Check(spSerializer::IndexReferenceForAnalysis(manager, &animation), "index animation through dispatcher");
        auto* fat = manager.GetFATForAnalysis();
        auto* entry = fat->FindByObjectForAnalysis(animation);
        Check(entry && entry->id == 1 && entry->classID == spAnimation::ClassID && entry->name.empty(),
              "actual engine RTTI suppresses physical animation name");
        Check(!entry->payloadWritten && entry->offset == 0 && entry->size == 0 &&
              fat->GetNextResourceIDForAnalysis() == 2, "initial save entry");
        Check(spSerializer::IndexReferenceForAnalysis(manager, &animation) && fat->GetResourceCountForAnalysis() == 1 &&
              fat->GetNextResourceIDForAnalysis() == 2, "duplicate index succeeds without new ID");
        Check(spSerializer::WriteReferenceForAnalysis(manager, out, &animation, &error) && error.empty(), "write first reference");
        const auto first = out.Data();
        Check(first.size() == 63 && Word(first, 0) == 1 && Word(first, 4) == 55 &&
              Word(first, 8) == spAnimation::ClassID && Word(first, 12) == 0x4F4F4253,
              "native reference and object prefix");
        Check(out.writes == 38 && out.seeks == 5, "native stream operation counts");
        Check(entry->payloadWritten && entry->offset == 8 && entry->size == 55, "native entry extent");
        Check(animation.SetTotalTimeForAnalysis(5), "modify source after first write");
        Check(spSerializer::WriteReferenceForAnalysis(manager, out, &animation), "repeat reference");
        const auto repeated = out.Data();
        Check(repeated.size() == 71 && std::equal(first.begin(), first.end(), repeated.begin()) &&
              Word(repeated, 63) == 1 && Word(repeated, 67) == 0, "repeat references unchanged original payload");
        Check(spSerializer::WriteReferenceForAnalysis(manager, out, nullptr), "null reference");
        const auto complete = out.Data();
        Check(complete.size() == 75 && Word(complete, 71) == 0, "null reference has no size word");
        return complete;
    }
    void Failures()
    {
        for (unsigned failedWrite : {1U, 2U, 38U})
        {
            spSerializerManager manager;
            Register(manager);
            spAnimation animation;
            Check(spSerializer::IndexReferenceForAnalysis(manager, &animation), "failure index");
            Output out;
            Check(out.Open(nullptr), "failure output");
            out.failedWrite = failedWrite;
            std::string error;
            Check(!spSerializer::WriteReferenceForAnalysis(manager, out, &animation, &error) && !error.empty(),
                  "write failure propagates, including native-unchecked patch");
            auto* entry = manager.GetFATForAnalysis()->FindByObjectForAnalysis(animation);
            Check(entry->payloadWritten == (failedWrite == 38), "native early one-shot flag retained");
            Check(out.Data().size() == (failedWrite == 1 ? 0U : failedWrite == 2 ? 4U : 63U),
                  "partial output retained, no invented rollback");
        }
        for (unsigned failedSeek : {3U, 4U, 5U})
        {
            spSerializerManager manager;
            Register(manager);
            spAnimation animation;
            Check(spSerializer::IndexReferenceForAnalysis(manager, &animation), "seek failure index");
            Output out;
            Check(out.Open(nullptr), "seek failure output");
            out.failedSeek = failedSeek;
            std::string error;
            Check(!spSerializer::WriteReferenceForAnalysis(manager, out, &animation, &error) && !error.empty(),
                  "host reports native-unchecked final seek failures");
            auto* entry = manager.GetFATForAnalysis()->FindByObjectForAnalysis(animation);
            Check(entry->payloadWritten && entry->offset == 8, "failed context stays explicitly poisoned");
        }
        spSerializerManager unavailable;
        spAnimation animation;
        Check(!spSerializer::IndexReferenceForAnalysis(unavailable, &animation), "missing dispatcher host guard");
    }

    void PreparedNodeReferences()
    {
        // Original fresh PC captures: authoring-palette-prebind/id7.json and
        // id1373.json. Only the next-ID/retained-state inputs are host supplied;
        // the same shared Node, FAT indexer and reference writer are exercised.
        for (const std::uint32_t id : {7U, 1373U})
        {
            spSerializerManager manager;
            manager.SetDispatchContextForAnalysis(2, 2);
            Check(manager.RegisterForAnalysis(spNode::ClassID,
                  std::make_shared<spNodeSerializer>(), 0xFF, 3), "register actual Node serializer for retained references");
            spNode node, followingNode;
            auto* fat = manager.GetFATForAnalysis();
            std::string error = "stale diagnostic";
            Check(fat && fat->SetNextResourceIDForAnalysis(id, &error) && error.empty(),
                  "host prepares original ID before indexing");
            Check(fat->GetNextResourceIDForAnalysis() == id && fat->GetResourceCountForAnalysis() == 0
                  && fat->FirstForAnalysis() == nullptr && fat->FindByObjectForAnalysis(node) == nullptr
                  && fat->FindByIDForAnalysis(id) == nullptr, "preparation alone creates no FAT entry or object mapping");
            Check(fat->SetNextResourceIDForAnalysis(id), "same next ID is an allowed idempotent preparation");
            Check(fat->IndexObjectForAnalysis(spNode::ClassID, node), "unchanged indexer assigns prepared ID to actual Node");
            auto* entry = fat->FindByObjectForAnalysis(node);
            Check(entry && fat->FindByIDForAnalysis(id) == entry && entry->id == id
                  && entry->object == &node && entry->classID == spNode::ClassID
                  && entry->fileID == 0 && !entry->payloadWritten && entry->offset == 0 && entry->size == 0,
                  "both actual FAT maps share the prepared save entry and native defaults");
            Check(fat->GetNextResourceIDForAnalysis() == id + 1 && fat->GetResourceCountForAnalysis() == 1
                  && fat->FirstForAnalysis() == entry && fat->NextForAnalysis() == nullptr,
                  "original indexer increments once and inserts one ordered entry");
            Check(!fat->IndexObjectForAnalysis(spNode::ClassID, node)
                  && fat->GetNextResourceIDForAnalysis() == id + 1 && fat->GetResourceCountForAnalysis() == 1,
                  "duplicate object pointer refuses without consuming the next ID");
            for (const auto rejected : {0U, id, std::numeric_limits<std::uint32_t>::max()})
            {
                Check(!fat->SetNextResourceIDForAnalysis(rejected, &error) && !error.empty(),
                      "zero, reverse and wrapping ID preparations explicitly refuse");
                Check(fat->GetNextResourceIDForAnalysis() == id + 1 && fat->GetResourceCountForAnalysis() == 1
                      && fat->FindByObjectForAnalysis(node) == entry && fat->FindByIDForAnalysis(id) == entry
                      && fat->FirstForAnalysis() == entry && fat->NextForAnalysis() == nullptr
                      && entry->id == id && entry->object == &node && !entry->payloadWritten
                      && entry->offset == 0 && entry->size == 0,
                      "rejected preparation preserves next ID, maps, list and entry state");
            }
            entry->offset = 0x100;
            entry->size = 25;
            entry->payloadWritten = true;
            Output out;
            Check(out.Open(nullptr), "open prepared Node reference output");
            Check(spSerializer::WriteReferenceForAnalysis(manager, out, &node, &error) && error.empty(),
                  "common writer accepts actual retained Node context");
            const Bytes expected = id == 7 ? Bytes{7, 0, 0, 0, 0, 0, 0, 0}
                                          : Bytes{0x5D, 5, 0, 0, 0, 0, 0, 0};
            Check(out.Data() == expected && out.writes == 2 && out.seeks == 0,
                  "prepared reference equals original PC eight-byte capture without payload/header/seek");
            Check(entry->id == id && entry->object == &node && entry->payloadWritten
                  && entry->offset == 0x100 && entry->size == 25 && fat->GetNextResourceIDForAnalysis() == id + 1
                  && fat->GetResourceCountForAnalysis() == 1 && fat->FindByObjectForAnalysis(node) == entry
                  && fat->FindByIDForAnalysis(id) == entry,
                  "reference writer preserves prepared original identity and retained extent");

            Check(fat->FirstForAnalysis() == entry && fat->SetNextResourceIDForAnalysis(id + 3, &error)
                  && error.empty() && fat->NextForAnalysis() == nullptr,
                  "forward preparation on a populated FAT preserves its cursor");
            Check(fat->IndexObjectForAnalysis(spNode::ClassID, followingNode)
                  && fat->FindByIDForAnalysis(id + 3) == fat->FindByObjectForAnalysis(followingNode)
                  && fat->GetNextResourceIDForAnalysis() == id + 4 && fat->GetResourceCountForAnalysis() == 2
                  && fat->FirstForAnalysis() == entry
                  && fat->NextForAnalysis() == fat->FindByObjectForAnalysis(followingNode)
                  && fat->NextForAnalysis() == nullptr,
                  "forward gap indexes through the same maps and preserves insertion order");
        }

        spResourceFATHelperForAnalysis boundary;
        spNode lastNode;
        constexpr auto maximum = std::numeric_limits<std::uint32_t>::max();
        Check(!boundary.SetNextResourceIDForAnalysis(0) && !boundary.SetNextResourceIDForAnalysis(maximum)
              && boundary.GetNextResourceIDForAnalysis() == 1 && boundary.GetResourceCountForAnalysis() == 0,
              "fresh invalid preparation also leaves the initial FAT state unchanged");
        Check(boundary.SetNextResourceIDForAnalysis(maximum - 1)
              && boundary.IndexObjectForAnalysis(spNode::ClassID, lastNode)
              && boundary.GetNextResourceIDForAnalysis() == maximum
              && boundary.FindByIDForAnalysis(maximum - 1) == boundary.FindByObjectForAnalysis(lastNode),
              "largest accepted preparation permits one index increment without wrap");
        Check(!boundary.SetNextResourceIDForAnalysis(maximum) && !boundary.SetNextResourceIDForAnalysis(maximum - 1)
              && !boundary.SetNextResourceIDForAnalysis(0)
              && boundary.GetNextResourceIDForAnalysis() == maximum && boundary.GetResourceCountForAnalysis() == 1,
              "exhausted prepared-ID range refuses every tested successor without changing state");
    }
}
int main(int argc, char** argv)
{
    try
    {
        const auto bytes = Success();
        Failures();
        PreparedNodeReferences();
        if (argc == 2 && std::strcmp(argv[1], "--emit") == 0)
        {
            std::cout << "REFERENCE_OUTPUT_HEX ";
            for (auto value : bytes) std::cout << std::hex << std::setw(2) << std::setfill('0') << unsigned(value);
            std::cout << '\n' << std::dec;
        }
        std::cout << "PASS " << checks << '/' << checks << ": native save-reference reconstruction\n";
        return 0;
    }
    catch (const std::exception& error) { std::cerr << error.what() << '\n'; return 1; }
}
