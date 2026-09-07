#include "Code/Sparkplug/spDataBlockSerializer.h"
#include "Code/Sparkplug/spAnimationSerializer.h"
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
    using Code = spDataBlockSerializer::SizeCode;
    int checks = 0;
    void Check(bool result, const char* message)
    {
        ++checks;
        if (!result) throw std::runtime_error(message);
    }
    Bytes Data(spMemoryStream& stream)
    {
        std::uint32_t size = 0;
        Check(stream.GetSize(&size), "size");
        const auto* data = static_cast<const std::uint8_t*>(stream.GetBuffer());
        return size ? Bytes(data, data + size) : Bytes{};
    }
    Bytes Header(unsigned id, unsigned size, unsigned code)
    {
        Bytes result{static_cast<std::uint8_t>((code << 5) | (id < 31 ? id : 31))};
        if (id >= 31) result.push_back(static_cast<std::uint8_t>(id));
        const unsigned width = 1U << (code - 5);
        for (unsigned i = 0; i < width; ++i)
            result.push_back(static_cast<std::uint8_t>(size >> (i * 8)));
        return result;
    }
    std::vector<Bytes> Success()
    {
        std::vector<Bytes> results;
        for (unsigned code : {5U, 6U, 7U})
        {
            for (const auto item : {std::pair{0U, 1U}, {3U, 8U}, {30U, 3U},
                                   {255U, 255U}, {42U, code == 5 ? 254U : 256U}})
            {
                spMemoryStream out;
                Check(out.Open(nullptr), "open");
                spDataBlockSerializer writer;
                Check(writer.BeginObjectForAnalysis(out), "begin object");
                Check(Data(out).empty(), "begin object no output");
                Check(writer.WriteBeginForAnalysis(item.first, static_cast<Code>(code)), "begin field");
                Check(Data(out) == Header(item.first, 0xFFFFFFFFU, code), "all-one reservation");
                Bytes payload(item.second);
                for (unsigned i = 0; i < item.second; ++i) payload[i] = static_cast<std::uint8_t>(i % 251);
                Check(out.WriteData(payload.data(), item.second), "payload");
                Check(writer.WriteEndForAnalysis(99), "end ignores supplied ID");
                Check(writer.GetOpenFieldCountForAnalysis() == 0, "popped");
                auto expected = Header(item.first, item.second, code);
                expected.insert(expected.end(), payload.begin(), payload.end());
                Check(Data(out) == expected, "same reserved width patched");
                std::uint32_t end = 0;
                Check(out.GetCurrentPosition(end) && end == expected.size(), "end position restored");
                results.push_back(Data(out));
                Check(writer.FinalizeObjectForAnalysis(), "terminator");
                Check(Data(out).back() == 0, "terminator byte");
            }
        }
        for (unsigned code : {5U, 6U, 7U})
        {
            spMemoryStream out;
            Check(out.Open(nullptr), "nested open");
            spDataBlockSerializer writer;
            Check(writer.BeginObjectForAnalysis(out), "nested object");
            Check(writer.WriteBeginForAnalysis(42, static_cast<Code>(code)), "outer begin");
            Check(out.WriteData("A", 1), "outer byte");
            Check(writer.WriteBeginForAnalysis(3, static_cast<Code>(code)), "inner begin");
            Check(out.WriteData("bc", 2), "inner bytes");
            Check(writer.WriteEndForAnalysis(99), "inner end");
            Check(out.WriteData("D", 1), "outer tail");
            Check(writer.WriteEndForAnalysis(99), "outer end");
            auto inner = Header(3, 2, code);
            inner.insert(inner.end(), {'b', 'c'});
            auto expected = Header(42, static_cast<unsigned>(inner.size()) + 2, code);
            expected.push_back('A');
            expected.insert(expected.end(), inner.begin(), inner.end());
            expected.push_back('D');
            Check(Data(out) == expected, "uniform-width nested encoding");
            results.push_back(Data(out));
        }
        return results;
    }
    class FailingStream final : public spStream
    {
    public:
        spMemoryStream memory;
        unsigned writes = 0, failWrite = 0;
        bool Open(const char* name) override { return memory.Open(name); }
        bool Open(std::uint32_t mode, const char* name) override { return memory.Open(mode, name); }
        bool Close() override { return memory.Close(); }
        bool Seek(SeekSource source, std::int32_t offset) override { return memory.Seek(source, offset); }
        bool GetCurrentPosition(std::uint32_t& position) const override { return memory.GetCurrentPosition(position); }
        bool ReadData(void* destination, std::uint32_t size) override { return memory.ReadData(destination, size); }
        bool WriteData(const void* data, std::uint32_t size) override
        {
            return ++writes != failWrite && memory.WriteData(data, size);
        }
        bool vfunc_WriteFromStream(spStream* source, std::uint32_t size) override
        { return memory.vfunc_WriteFromStream(source, size); }
        bool GetSize(std::uint32_t* size) const override { return memory.GetSize(size); }
    };
    void FailuresAndHostGuards()
    {
        for (unsigned failedCall : {1U, 9U})
        {
            FailingStream out;
            Check(out.Open(nullptr), "animation write failure stream");
            out.failWrite = failedCall;
            spAnimation empty;
            std::string error;
            Check(!spAnimationSerializer{}.WriteFieldsForAnalysis(out, empty, &error) && !error.empty(),
                  "animation write failure propagates");
            Check(out.writes == failedCall && Data(out.memory).size() == (failedCall == 1 ? 0 : 17),
                  "native empty writer partial output and stop count");
        }
        for (bool beginFailure : {true, false})
        {
            FailingStream out;
            Check(out.Open(nullptr), "failure open");
            spDataBlockSerializer writer;
            Check(writer.BeginObjectForAnalysis(out), "failure object");
            if (beginFailure)
            {
                out.failWrite = 2;
                Check(!writer.WriteBeginForAnalysis(42), "begin failure");
                Check(Data(out.memory) == Bytes{0xFF}, "partial begin retained");
                Check(!writer.WriteEndForAnalysis(99), "host guard against unfinished begin");
            }
            else
            {
                Check(writer.WriteBeginForAnalysis(42), "before end failure");
                Check(out.WriteData("abc", 3), "before end payload");
                const auto before = Data(out.memory);
                out.failWrite = out.writes + 2;
                Check(!writer.WriteEndForAnalysis(99), "end failure");
                Check(Data(out.memory) == before, "failed patch bytes retained");
            }
            std::uint32_t position = 0;
            Check(out.GetCurrentPosition(position) && position == 1, "no restore after failed header write");
            Check(writer.GetOpenFieldCountForAnalysis() == 1, "failed write retains open header");
            Check(!writer.FinalizeObjectForAnalysis(), "host refuses finalize unfinished stack");
        }
        spMemoryStream out, other;
        Check(out.Open(nullptr) && other.Open(nullptr), "guard streams");
        spDataBlockSerializer writer;
        Check(!writer.WriteBeginForAnalysis(1) && !writer.WriteEndForAnalysis(1) &&
              !writer.FinalizeObjectForAnalysis(), "unbound host guards");
        Check(writer.BeginObjectForAnalysis(out), "guard object");
        Check(!writer.WriteBeginForAnalysis(31) && !writer.WriteBeginForAnalysis(256) &&
              !writer.WriteBeginForAnalysis(1, Code::Fixed8), "invalid reservations rejected before output");
        Check(Data(out).empty(), "reservation rejection unchanged output");
        Check(writer.WriteBeginForAnalysis(42, Code::UInt8), "guard begin");
        const auto before = Data(out);
        Check(!writer.WriteBeginForAnalysis(3, Code::UInt32), "mixed nesting host guard");
        Check(!writer.BeginObjectForAnalysis(other), "no retarget of open stack");
        Check(!writer.WriteEndForAnalysis(42) && Data(out) == before, "empty field host rejection");
        Bytes large(256, 1);
        Check(out.WriteData(large.data(), 256), "overflow payload");
        Check(!writer.WriteEndForAnalysis(42), "too narrow reservation host rejection");
        Check(writer.GetOpenFieldCountForAnalysis() == 1, "overflow retains unresolved header");

        spMemoryStream wire;
        Check(wire.Open(nullptr), "direct safe writer");
        Check(writer.WriteFieldForAnalysis(wire, 31, nullptr, 0), "explicit direct wire correction");
        Check(Data(wire) == Bytes({0xBF, 31, 0}), "valid empty field31 differs from native bugs");
        Check(wire.Seek(spStream::SeekSource::essStart, 0), "wire rewind");
        const auto* decoded = writer.ReadHeaderForAnalysis(wire);
        Check(decoded && decoded->fieldID == 31 && decoded->payloadSize == 0, "safe encoding accepted by wire grammar");
    }
}
int main(int argc, char** argv)
{
    try
    {
        const auto outputs = Success();
        FailuresAndHostGuards();
        if (argc == 2 && std::strcmp(argv[1], "--emit") == 0)
        {
            std::cout << "BLOCK_OUTPUTS [";
            bool first = true;
            for (const auto& bytes : outputs)
            {
                if (!first) std::cout << ',';
                first = false;
                std::cout << '"';
                for (const auto value : bytes)
                    std::cout << std::hex << std::setw(2) << std::setfill('0') << unsigned(value);
                std::cout << '"';
            }
            std::cout << "]\n" << std::dec;
        }
        std::cout << "PASS " << checks << "/" << checks << ": PC block writer reconstruction\n";
        return 0;
    }
    catch (const std::exception& error) { std::cerr << error.what() << '\n'; return 1; }
}
