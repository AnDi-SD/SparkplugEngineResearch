#include "Code/Sparkplug/spSubtitleTrack.h"
#include "Code/SparkBase/spMemoryStream.h"
#include "Analysis/PC/spSubtitleTrackAbi.h"
#include <cstring>
#include <iostream>
#include <sstream>
#include <stdexcept>
#include <string>

using namespace sparkplug::reconstruction;
namespace
{
    int checks = 0;
    void Check(bool value, const char* message)
    {
        ++checks;
        if (!value)
            throw std::runtime_error(message);
    }
    std::string Hex(const std::vector<std::uint8_t>& bytes)
    {
        constexpr char alphabet[] = "0123456789abcdef";
        std::string result;
        for (auto value : bytes)
        {
            result += alphabet[value >> 4];
            result += alphabet[value & 15];
        }
        return result;
    }
    std::vector<std::uint8_t> Decode(const std::string& hex)
    {
        if (hex.size() % 2)
            throw std::runtime_error("Odd fixture hex");
        const auto nibble = [](char value) -> unsigned {
            if (value >= '0' && value <= '9')
                return unsigned(value - '0');
            if (value >= 'a' && value <= 'f')
                return unsigned(value - 'a' + 10);
            throw std::runtime_error("Invalid fixture hex");
        };
        std::vector<std::uint8_t> result;
        for (std::size_t index = 0; index < hex.size(); index += 2)
            result.push_back(std::uint8_t(nibble(hex[index]) * 16 + nibble(hex[index + 1])));
        return result;
    }
    void Prepare(spMemoryStream& stream, const std::vector<std::uint8_t>& data)
    {
        if (!stream.Open("subtitle-fixture") || !stream.ResizeAndSetSize(std::uint32_t(data.size())))
            throw std::runtime_error("Fixture stream allocation");
        if (!data.empty())
            std::memcpy(stream.GetBuffer(), data.data(), data.size());
    }
    std::int64_t Offset(const spSubtitleTrack& track, const char* pointer)
    {
        return pointer ? pointer - reinterpret_cast<const char*>(track.GetBlobForAnalysis().data()) : -1;
    }
    void Case(const std::string& line)
    {
        std::istringstream in(line);
        std::string hex;
        std::uint32_t selected;
        unsigned opaque;
        if (!(in >> hex >> selected >> opaque))
            throw std::runtime_error("Missing subtitle fixture");
        spMemoryStream stream;
        Prepare(stream, Decode(hex));
        spSubtitleTrack track;
        track.SetSelectedTableForAnalysis(selected);
        track.SetOpaqueByteForAnalysis(std::uint8_t(opaque));
        if (!track.LoadForAnalysis(stream))
            throw std::runtime_error("Invalid native subtitle fixture");
        std::cout << "{\"records\":[";
        bool separator = false;
        for (const auto& row : track.GetRecordsForAnalysis())
        {
            if (separator)
                std::cout << ',';
            separator = true;
            std::cout << '[' << row[0] << ',' << row[1] << ',' << row[2] << ',' << row[3] << ']';
        }
        std::cout << "],\"blob\":\"" << Hex(track.GetBlobForAnalysis()) << "\",\"tables\":[";
        separator = false;
        for (const auto& table : track.GetTablesForAnalysis())
        {
            if (separator)
                std::cout << ',';
            separator = true;
            std::cout << '[';
            bool entrySeparator = false;
            for (const auto& entry : table)
            {
                if (entrySeparator)
                    std::cout << ',';
                entrySeparator = true;
                std::cout << '[' << entry.key << ',' << entry.blobOffset << ']';
            }
            std::cout << ']';
        }
        std::cout << "],\"selected\":" << track.GetSelectedTableForAnalysis() <<
            ",\"opaque\":" << unsigned(track.GetOpaqueByteForAnalysis()) << ",\"lookup\":[";
        std::uint32_t key;
        separator = false;
        while (in >> key)
        {
            if (separator)
                std::cout << ',';
            separator = true;
            std::cout << Offset(track, track.LookupForAnalysis(key));
        }
        std::uint32_t consumed = 0;
        if (!stream.GetCurrentPosition(consumed))
            throw std::runtime_error("Missing consumed stream position");
        spCloneManager clones;
        auto clone = clones.Clone(track);
        auto* typed = dynamic_cast<spSubtitleTrack*>(clone.get());
        if (!typed || !typed->GetRecordsForAnalysis().empty() || !typed->GetBlobForAnalysis().empty() ||
            !typed->GetTablesForAnalysis().empty() || typed->GetSelectedTableForAnalysis() != 0 ||
            typed->GetOpaqueByteForAnalysis() != 0)
            throw std::runtime_error("Clone carries own payload");
        track.ClearForAnalysis();
        if (!track.GetRecordsForAnalysis().empty() || !track.GetBlobForAnalysis().empty() ||
            !track.GetTablesForAnalysis().empty() || track.GetSelectedTableForAnalysis() != 0 ||
            track.GetOpaqueByteForAnalysis() != 0)
            throw std::runtime_error("Clear carries own payload");
        std::cout << "],\"consumed\":" << consumed << "}\n";
    }
    void MainChecks()
    {
        // One opaque record, byte blob a\0b\0, two tables. First table has
        // duplicate key7 at different offsets; native lookup returns first.
        const auto payload = Decode("01000000070000000000803f0000004009000000"
            "0400000061006200020000000200000007000000000000000700000002000000"
            "010000000700000002000000");
        spSubtitleTrack track;
        Check(!track.IsLoadedForAnalysis() && track.GetSelectedTableForAnalysis() == 0 &&
            track.GetOpaqueByteForAnalysis() == 0 && track.GetRecordsForAnalysis().empty(),
            "factory empty payload");
        track.SetName("subtitle-name");
        track.SetOpaqueByteForAnalysis(255);
        spMemoryStream stream;
        Prepare(stream, payload);
        Check(track.LoadForAnalysis(stream) && track.GetOpaqueByteForAnalysis() == 255,
            "reader loads payload and preserves opaque byte");
        Check(track.GetRecordsForAnalysis()[0] == spSubtitleTrack::RecordWordsForAnalysis{7, 0x3F800000, 0x40000000, 9},
            "reader preserves four raw record words");
        Check(std::string(track.LookupForAnalysis(7)) == "a" && !track.LookupForAnalysis(9),
            "lookup first duplicate and missing key");
        track.SetSelectedTableForAnalysis(1);
        Check(std::string(track.LookupForAnalysis(7)) == "b", "selected table changes lookup");
        spCloneManager clones;
        auto clone = clones.Clone(track);
        auto* typed = dynamic_cast<spSubtitleTrack*>(clone.get());
        Check(typed && std::string(typed->GetName()) == "subtitle-name" &&
            typed->GetTablesForAnalysis().empty() && typed->GetOpaqueByteForAnalysis() == 0 &&
            typed->GetSelectedTableForAnalysis() == 0, "named clone copies only name");
        Check(!track.LoadForAnalysis(stream), "host rejects repeated load before clear");
        track.ClearForAnalysis();
        Check(!track.IsLoadedForAnalysis() && track.GetTablesForAnalysis().empty() &&
            track.GetBlobForAnalysis().empty() && track.GetRecordsForAnalysis().empty() &&
            track.GetSelectedTableForAnalysis() == 0 && track.GetOpaqueByteForAnalysis() == 0 &&
            std::string(track.GetName()) == "subtitle-name", "clear resets own storage while name remains");
        Prepare(stream, Decode("ffffffff"));
        Check(!track.LoadForAnalysis(stream) && !track.IsLoadedForAnalysis(),
            "host rejects overflowing/truncated count before publication");
        Prepare(stream, Decode("00000000010000000001000000010000000700000001000000"));
        Check(!track.LoadForAnalysis(stream) && track.GetTablesForAnalysis().empty(),
            "host rejects blob offset outside allocation");
        Prepare(stream, payload);
        Check(!track.LoadForAnalysis(stream, 8) && !track.IsLoadedForAnalysis(),
            "explicit host byte budget");
        Prepare(stream, payload);
        Check(track.LoadForAnalysis(stream), "clear allows fresh successful read");
        Check(track.vfunc_18().IsExactly(spSubtitleTrack::ClassID) &&
            track.vfunc_18().IsKindOf(spNamedObject::ClassID), "native RTTI ancestry");
        std::cout << "PASS " << checks << "/" << checks << ": subtitle read,lookup,clone,clear\n";
    }
}
int main(int argc, char** argv)
{
    try
    {
        if (argc == 2 && std::string(argv[1]) == "--batch")
        {
            std::string line;
            while (std::getline(std::cin, line))
                if (!line.empty())
                    Case(line);
        }
        else
            MainChecks();
        return 0;
    }
    catch (const std::exception& error)
    {
        std::cerr << "FAIL: " << error.what() << '\n';
        return 1;
    }
}
