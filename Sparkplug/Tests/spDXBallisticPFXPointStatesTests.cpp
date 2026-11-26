#include "Code/SparkplugDX/spDXBallisticPFX.h"

#include <array>
#include <charconv>
#include <cstddef>
#include <cstdint>
#include <cstring>
#include <iomanip>
#include <iostream>
#include <sstream>
#include <stdexcept>
#include <string>
#include <string_view>

using sparkplug::reconstruction::spDXBallisticPFX;

namespace
{
    using Words = std::array<std::uint32_t, 4>;

    constexpr Words Indices{156, 157, 155, 154};
    constexpr Words Values{1, 1, 0x3F800000, 0x3F800000};

    void Require(bool condition, const char* message)
    {
        if (!condition)
            throw std::runtime_error(message);
    }

    struct Submission
    {
        std::uint32_t index;
        std::uint32_t value;
        std::uint32_t prior;
        std::uint32_t status;
        std::size_t readOrdinal;
    };

    struct StateFixture
    {
        std::array<std::uint32_t, 256> cache{};
        Words statuses;
        std::array<Submission, 4> submissions{};
        std::size_t resolverReads = 0;
        std::size_t submissionCount = 0;
        std::size_t unavailableAtRead = 0;
        bool provideSubmit = true;
        bool unexpectedSubmit = false;

        StateFixture(const Words& initial, const Words& results) : statuses(results)
        {
            for (std::size_t index = 0; index < cache.size(); ++index)
                cache[index] = 0xC0DEC000u | static_cast<std::uint32_t>(index);
            for (std::size_t lane = 0; lane < Indices.size(); ++lane)
                cache[Indices[lane]] = initial[lane];
        }

        static spDXBallisticPFX::BorrowedRenderStatesForAnalysis ReadRenderer(void* context) noexcept
        {
            auto& fixture = *static_cast<StateFixture*>(context);
            ++fixture.resolverReads;
            if (fixture.resolverReads == fixture.unavailableAtRead)
                return {};
            return {fixture.cache.data(), fixture.cache.size(),
                    fixture.provideSubmit ? Submit : nullptr, context};
        }

        static std::int32_t Submit(void* context, std::uint32_t index, std::uint32_t value) noexcept
        {
            auto& fixture = *static_cast<StateFixture*>(context);
            std::size_t lane = 0;
            while (lane < Indices.size() && Indices[lane] != index)
                ++lane;
            if (lane == Indices.size() || fixture.submissionCount == fixture.submissions.size())
            {
                fixture.unexpectedSubmit = true;
                return -1;
            }

            const auto status = fixture.statuses[lane];
            fixture.submissions[fixture.submissionCount++] =
                {index, value, fixture.cache[index], status, fixture.resolverReads};
            std::int32_t signedStatus;
            std::memcpy(&signedStatus, &status, sizeof(status));
            return signedStatus;
        }

        bool Apply(std::uint32_t primary = spDXBallisticPFX::CapturedPrimary) noexcept
        {
            return spDXBallisticPFX::ApplyPointStatesForAnalysis(primary, ReadRenderer, this);
        }
    };

    void Check(const StateFixture& fixture, const Words& initial)
    {
        Require(!fixture.unexpectedSubmit && fixture.resolverReads == 4, "wrong renderer read count");
        std::size_t emitted = 0;
        for (std::size_t lane = 0; lane < Indices.size(); ++lane)
        {
            const auto index = Indices[lane];
            Require(fixture.cache[index] == Values[lane], "wrong final render state");
            if (initial[lane] == Values[lane])
                continue;
            Require(emitted < fixture.submissionCount, "changed render state was skipped");
            const auto& call = fixture.submissions[emitted++];
            Require(call.index == index && call.value == Values[lane] && call.prior == initial[lane]
                        && call.status == fixture.statuses[lane] && call.readOrdinal == lane + 1,
                    "wrong device order or timing relative to the cache write");
        }
        Require(emitted == fixture.submissionCount, "cache hit submitted");
        for (std::uint32_t index = 0; index < fixture.cache.size(); ++index)
        {
            if (index < 154 || index > 157)
                Require(fixture.cache[index] == (0xC0DEC000u | index), "unrelated render cache slot changed");
        }
    }

    std::uint32_t ReadWord(std::istringstream& stream)
    {
        std::string word;
        Require(bool(stream >> word) && word.size() == 8, "expected a raw DWORD");
        std::uint32_t value = 0;
        const auto end = word.data() + word.size();
        const auto decoded = std::from_chars(word.data(), end, value, 16);
        Require(decoded.ec == std::errc{} && decoded.ptr == end, "invalid raw DWORD");
        return value;
    }

    void WriteRow(const StateFixture& fixture)
    {
        std::cout << std::dec << fixture.resolverReads << ' ' << std::hex << std::setfill('0');
        for (const auto entry : fixture.cache)
        {
            for (unsigned shift = 0; shift < 32; shift += 8)
                std::cout << std::setw(2) << ((entry >> shift) & 0xFFu);
        }
        std::cout << ' ' << std::dec << fixture.submissionCount << std::hex;
        for (std::size_t index = 0; index < fixture.submissionCount; ++index)
        {
            const auto& call = fixture.submissions[index];
            std::cout << ' ' << std::setw(8) << call.index << ' ' << std::setw(8) << call.value
                      << ' ' << std::setw(8) << call.prior << ' ' << std::setw(8) << call.status;
        }
        std::cout << '\n';
    }

    void Replay()
    {
        for (std::string line; std::getline(std::cin, line);)
        {
            std::istringstream stream(line);
            Words initial, statuses;
            for (auto& word : initial)
                word = ReadWord(stream);
            for (auto& word : statuses)
                word = ReadWord(stream);
            std::string extra;
            Require(!(stream >> extra), "extra replay input");
            StateFixture fixture(initial, statuses);
            Require(fixture.Apply(), "qualified replay binding refused");
            Check(fixture, initial);
            WriteRow(fixture);
        }
        Require(std::cin.eof() && bool(std::cout), "replay I/O failed");
    }

    void CheckBindings()
    {
        const Words errors{0x80004005, 1, 0xFFFFFFFF, 0x8876086C};
        for (const auto& initial : {Words{}, Values, Words{1, 0, 0x3F800000, 0}})
        {
            StateFixture fixture(initial, errors);
            Require(fixture.Apply(), "valid renderer binding refused");
            Check(fixture, initial);
        }

        StateFixture ignored(Values, errors);
        ignored.provideSubmit = false;
        Require(ignored.Apply(), "hot cache required a device submission");
        Check(ignored, Values);

        StateFixture missing(Words{}, errors);
        missing.unavailableAtRead = 2;
        Require(!missing.Apply() && missing.resolverReads == 2 && missing.submissionCount == 1
                    && missing.cache[156] == 1 && missing.cache[157] == 0,
                "missing renderer binding did not preserve prior progress");

        StateFixture wrong(Words{}, errors);
        Require(!wrong.Apply(0) && wrong.resolverReads == 0 && wrong.submissionCount == 0,
                "wrong primary reached a renderer binding");
    }
}

int main(int argc, char** argv)
{
    try
    {
        if (argc == 2 && std::string_view(argv[1]) == "--replay")
            Replay();
        else if (argc == 1)
            CheckBindings();
        else
            return 2;
        return 0;
    }
    catch (const std::exception& error)
    {
        std::cerr << error.what() << '\n';
        return 1;
    }
}
