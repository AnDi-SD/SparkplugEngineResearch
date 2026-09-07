#include "Code/Sparkplug/spAnimationSerializer.h"
#include "Code/Sparkplug/spSerializerManager.h"
#include "Code/Sparkplug/spResourceFATSerializer.h"
#include "Code/SparkBase/spMemoryStream.h"
#include <cstring>
#include <iomanip>
#include <iostream>
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
}
int main(int argc, char** argv)
{
    try
    {
        const auto bytes = Success();
        Failures();
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
