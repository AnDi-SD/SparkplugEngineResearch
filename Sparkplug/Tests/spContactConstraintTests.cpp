#include "Code/Sparkplug/spContactConstraint.h"
#include "Analysis/PC/spContactConstraintAbi.h"
#include <algorithm>
#include <array>
#include <iomanip>
#include <iostream>
#include <sstream>
#include <stdexcept>
#include <string>
#include <type_traits>

using namespace sparkplug::reconstruction;
namespace
{
    using Raw = spContactConstraint::CapturedStateForAnalysis;
    using Profile = spContactConstraint::ProfileForAnalysis;
    using Result = spContactConstraint::ResultForAnalysis;
    constexpr auto Admitted = Profile::PcCapturedFiniteOwn1C;
    unsigned checks = 0;
    void Check(const bool value, const char* message)
    { ++checks; if (!value) throw std::runtime_error(message); }
    std::uint32_t Word(const Raw& state, const std::size_t offset)
    {
        std::uint32_t result = 0;
        for (unsigned byte = 0; byte != 4; ++byte) result |= std::uint32_t{state[offset + byte]} << (byte * 8);
        return result;
    }
    void Put(Raw& state, const std::size_t offset, const std::uint32_t value)
    { for (unsigned byte = 0; byte != 4; ++byte) state[offset + byte] = static_cast<std::uint8_t>(value >> (byte * 8)); }
    Raw Input(const std::uint8_t mode, const std::uint32_t first, const std::uint32_t second)
    {
        Raw state;
        for (std::size_t index = 0; index != state.size(); ++index)
            state[index] = static_cast<std::uint8_t>(0xA5U + index * 37U);
        Put(state, 0, sparkplug::evidence::pc::spContactConstraintPrimary);
        Put(state, 0x58, 0xDEADBEEF); Put(state, 0x5C, 0x12345678);
        state[0x88] = mode; Put(state, 0x8C, first); Put(state, 0x90, second);
        return state;
    }
    struct Guarded final
    {
        std::array<std::uint8_t, 16> before;
        spContactConstraint value;
        std::array<std::uint8_t, 16> after;
        Guarded(const Raw& input, const Profile profile) : value(input, profile)
        { before.fill(0xD7); after.fill(0xE3); }
        void VerifyCanaries() const
        {
            Check(std::all_of(before.begin(), before.end(), [](auto byte) { return byte == 0xD7; }), "leading source-object canary");
            Check(std::all_of(after.begin(), after.end(), [](auto byte) { return byte == 0xE3; }), "trailing source-object canary");
        }
    };
    void ExpectedCall(const Raw& input, const std::uint32_t count58, const std::uint32_t count5C,
        const std::uint32_t firstAfter, const std::uint32_t secondAfter)
    {
        Guarded guarded(input, Admitted);
        auto expected = input; Put(expected, 0x58, count58); Put(expected, 0x5C, count5C);
        Put(expected, 0x8C, firstAfter); Put(expected, 0x90, secondAfter);
        Check(guarded.value.OwnSlot1CForAnalysis() == Result::Completed, "finite captured operation completes");
        Check(guarded.value.GetCapturedStateForAnalysis() == expected, "whole228B state and opaque spans match explicit expectation");
        guarded.VerifyCanaries();
        Check(guarded.value.OwnSlot1CForAnalysis() == Result::Completed
            && guarded.value.GetCapturedStateForAnalysis() == expected, "repeat operation preserves the complete prior result");
        guarded.VerifyCanaries();
    }
    using CopyInput = spContactConstraint::RawOwn24SourceForAnalysis;
    using CopyMember = Result (spContactConstraint::*)(CopyInput) noexcept;
    static_assert(std::is_same_v<decltype(&spContactConstraint::OwnSlot24ForAnalysis), CopyMember>);
    static_assert(static_cast<unsigned>(Profile::Unqualified) == 0);
    static_assert(static_cast<unsigned>(Profile::PcCapturedFiniteOwn1C) == 1);

    CopyInput CopyPayload(const std::uint8_t seed)
    {
        CopyInput data;
        for (std::size_t index = 0; index != data.size(); ++index)
            data[index] = static_cast<std::uint8_t>(seed + index * 37U);
        return data;
    }
    void CopyUnitTests()
    {
        const auto initial = Input(1, 0x3F000000, 0x7F7FFFFF);
        const auto payload = CopyPayload(0x5A);
        for (const auto profile : {Profile::Unqualified, Profile::PcCapturedFiniteOwn1C})
        {
            Guarded guarded(initial, profile);
            Check(guarded.value.OwnSlot24ForAnalysis(payload) == Result::UnqualifiedProfile
                && guarded.value.GetCapturedStateForAnalysis() == initial, "copy default/old1C-only profile refuses before mutation");
            guarded.VerifyCanaries();
        }
        for (const auto profile : {Profile::PcCapturedRawOwn24, Profile::PcCapturedFiniteOwn1CAndRawOwn24})
        {
            auto invalid = initial; Put(invalid, 0, 0x6E7094);
            Guarded guarded(invalid, profile);
            Check(guarded.value.OwnSlot24ForAnalysis(payload) == Result::CapturedPrimaryNotAdmitted
                && guarded.value.GetCapturedStateForAnalysis() == invalid, "copy incorrect primary tag preserves complete receiver");
            guarded.VerifyCanaries();
        }
        Guarded rawOnly(initial, Profile::PcCapturedRawOwn24);
        Check(rawOnly.value.OwnSlot1CForAnalysis() == Result::UnqualifiedProfile
            && rawOnly.value.GetCapturedStateForAnalysis() == initial, "rawcopy-only profile does not silently admit1C");
        rawOnly.VerifyCanaries();
        for (const auto seed : {0U, 0x17U, 0x5AU, 0x80U, 0xA5U, 0xFFU})
        {
            const auto input = CopyPayload(static_cast<std::uint8_t>(seed));
            auto expected = initial; std::copy(input.begin(), input.end(), expected.begin() + 0x88);
            Guarded guarded(initial, Profile::PcCapturedRawOwn24);
            Check(guarded.value.OwnSlot24ForAnalysis(input) == Result::Completed
                && guarded.value.GetCapturedStateForAnalysis() == expected, "raw92B copy preserves prefix00..87 and all exact copied bits");
            Check(input == CopyPayload(static_cast<std::uint8_t>(seed)), "caller source92B remains unchanged");
            guarded.VerifyCanaries();
            Check(guarded.value.OwnSlot24ForAnalysis(input) == Result::Completed
                && guarded.value.GetCapturedStateForAnalysis() == expected, "repeated rawcopy is exact/idempotent");
            guarded.VerifyCanaries();
        }
        CopyInput unusual;
        constexpr std::array<std::uint32_t, 6> words{0x7FC01234, 0x7F800001, 0xFF800000, 0x31000020, 0xFFFFFFFF, 0x80000000};
        for (std::size_t word = 0; word != 23; ++word)
            for (unsigned byte = 0; byte != 4; ++byte) unusual[word * 4 + byte] = static_cast<std::uint8_t>(words[word % words.size()] >> (byte * 8));
        Guarded changed(initial, Profile::PcCapturedRawOwn24);
        auto expected = initial; std::copy(unusual.begin(), unusual.end(), expected.begin() + 0x88);
        Check(changed.value.OwnSlot24ForAnalysis(unusual) == Result::Completed
            && changed.value.GetCapturedStateForAnalysis() == expected, "NaN/infinity/opaque-address words are copied without interpretation");
        const auto next = CopyPayload(0x3C); std::copy(next.begin(), next.end(), expected.begin() + 0x88);
        Check(changed.value.OwnSlot24ForAnalysis(next) == Result::Completed
            && changed.value.GetCapturedStateForAnalysis() == expected, "changed caller source replaces only raw92B ingress");
        changed.VerifyCanaries();

        // Portable profile composition checks are our admission contracts;
        // original24->1C composition requires its own native qualification.
        Guarded combined(initial, Profile::PcCapturedFiniteOwn1CAndRawOwn24);
        CopyInput finite{}; finite[0] = 1;
        for (unsigned byte = 0; byte != 4; ++byte)
        { finite[4 + byte] = static_cast<std::uint8_t>(0x3F000000U >> (byte * 8)); finite[8 + byte] = static_cast<std::uint8_t>(0x7F7FFFFFU >> (byte * 8)); }
        Check(combined.value.OwnSlot24ForAnalysis(finite) == Result::Completed
            && combined.value.OwnSlot1CForAnalysis() == Result::Completed
            && Word(combined.value.GetCapturedStateForAnalysis(), 0x58) == 3
            && Word(combined.value.GetCapturedStateForAnalysis(), 0x5C) == 1, "combined profile explicitly admits both independent operations");
        finite[4] = 1; finite[5] = 0; finite[6] = 0x80; finite[7] = 0x7F;
        Check(combined.value.OwnSlot24ForAnalysis(finite) == Result::Completed, "combined raw ingress admits nonfinite bits as data");
        const auto copied = combined.value.GetCapturedStateForAnalysis();
        Check(combined.value.OwnSlot1CForAnalysis() == Result::NonFiniteInput
            && combined.value.GetCapturedStateForAnalysis() == copied, "subsequent1C analytical finite refusal preserves copied state");
        combined.VerifyCanaries();
    }

    void UnitTests()
    {
        const auto finite = Input(1, 0x3F000000, 0x7F7FFFFF);
        spContactConstraint defaultProfile(finite);
        Check(defaultProfile.OwnSlot1CForAnalysis() == Result::UnqualifiedProfile
            && defaultProfile.GetCapturedStateForAnalysis() == finite, "default admission refusal preserves all captured bytes");
        auto wrongTag = finite; Put(wrongTag, 0, 0x6E7094);
        Guarded incorrect(wrongTag, Admitted);
        Check(incorrect.value.OwnSlot1CForAnalysis() == Result::CapturedPrimaryNotAdmitted
            && incorrect.value.GetCapturedStateForAnalysis() == wrongTag, "incorrect captured identity refuses before mutation");
        incorrect.VerifyCanaries();
        for (const auto mode : {0U, 1U})
            for (const auto nonfinite : {0x7F800000U, 0xFF800000U, 0x7FC01234U, 0x7F800001U})
                for (const auto offset : {0x8CU, 0x90U})
                {
                    auto input = finite; input[0x88] = static_cast<std::uint8_t>(mode); Put(input, offset, nonfinite);
                    Guarded guarded(input, Admitted);
                    Check(guarded.value.OwnSlot1CForAnalysis() == Result::NonFiniteInput
                        && guarded.value.GetCapturedStateForAnalysis() == input, "analytical nonfinite refusal preserves complete state even for mode0 raw90");
                    guarded.VerifyCanaries();
                }

        // Explicit boundary expectations supplement the independent native
        // replay matrix; no portable FPU exception/status result is asserted.
        ExpectedCall(Input(0, 0xBF800000, 0xC0000000), 1, 0, 0, 0xC0000000);
        ExpectedCall(Input(1, 0xBF800000, 0xC0000000), 1, 0, 0, 0);
        ExpectedCall(Input(0, 0x80000000, 0xFF7FFFFF), 1, 0, 0x80000000, 0xFF7FFFFF);
        ExpectedCall(Input(1, 0x80000000, 0), 1, 0, 0x80000000, 0);
        ExpectedCall(Input(0, 1, 0xBF800000), 3, 0, 1, 0xBF800000);
        ExpectedCall(Input(1, 1, 1), 3, 0, 1, 1);
        ExpectedCall(Input(1, 0x80000001, 0x807FFFFF), 1, 0, 0, 0);
        ExpectedCall(Input(1, 0x007FFFFF, 0x00800000), 3, 0, 0x007FFFFF, 0x00800000);
        ExpectedCall(Input(0, 0x7F7FFFFF, 0xFF7FFFFF), 3, 2, 0x7F7FFFFF, 0xFF7FFFFF);
        ExpectedCall(Input(1, 0x7F7FFFFF, 0), 2, 1, 0x7F7FFFFF, 0);
        ExpectedCall(Input(1, 0, 0x7F7FFFFF), 2, 1, 0, 0x7F7FFFFF);
        ExpectedCall(Input(1, 0x7F7FFFFF, 0x7F7FFFFF), 3, 2, 0x7F7FFFFF, 0x7F7FFFFF);
        for (unsigned bit = 1; bit != 8; ++bit)
        {
            ExpectedCall(Input(static_cast<std::uint8_t>(1U << bit), 0x3F000000, 0x7F7FFFFF), 3, 0, 0x3F000000, 0x7F7FFFFF);
            ExpectedCall(Input(static_cast<std::uint8_t>((1U << bit) | 1U), 0x3F000000, 0x7F7FFFFF), 3, 1, 0x3F000000, 0x7F7FFFFF);
        }
        Guarded changing(Input(1, 0xBF800000, 0xC0000000), Admitted);
        Check(changing.value.OwnSlot1CForAnalysis() == Result::Completed, "first state in changed-input sequence completes");
        auto& next = changing.value.CapturedStateForFixture();
        next[0x88] = 0xFE; Put(next, 0x8C, 0x80000000); Put(next, 0x90, 0xFF7FFFFF);
        const auto supplied = next;
        auto expected = supplied; Put(expected, 0x58, 1); Put(expected, 0x5C, 0);
        Check(changing.value.OwnSlot1CForAnalysis() == Result::Completed
            && changing.value.GetCapturedStateForAnalysis() == expected, "changed-input call rereads lowmodebit and signedzero; ignored raw90 stays exact");
        changing.VerifyCanaries();
        CopyUnitTests();
    }

    std::string Hex(const std::uint8_t* bytes, const std::size_t size)
    {
        constexpr char digits[] = "0123456789abcdef"; std::string result; result.reserve(size * 2);
        for (std::size_t index = 0; index != size; ++index)
        { result += digits[bytes[index] >> 4]; result += digits[bytes[index] & 15]; }
        return result;
    }
    unsigned Nibble(const char character)
    {
        if (character >= '0' && character <= '9') return static_cast<unsigned>(character - '0');
        if (character >= 'a' && character <= 'f') return static_cast<unsigned>(character - 'a' + 10);
        if (character >= 'A' && character <= 'F') return static_cast<unsigned>(character - 'A' + 10);
        throw std::runtime_error("replay input contains a nonhexadecimal character");
    }
    std::string Replay(const std::string& line)
    {
        // The portable facade has its own C++ layout. The replay projection
        // is the declared native228B state plus16-byte input/output canaries.
        if (line.size() != 520) throw std::runtime_error("replay requires exactly260 guarded input bytes");
        std::array<std::uint8_t, 260> bytes;
        for (std::size_t index = 0; index != bytes.size(); ++index)
            bytes[index] = static_cast<std::uint8_t>((Nibble(line[index * 2]) << 4) | Nibble(line[index * 2 + 1]));
        Raw input; std::copy_n(bytes.begin() + 16, input.size(), input.begin());
        Guarded guarded(input, Admitted);
        std::copy_n(bytes.begin(), 16, guarded.before.begin());
        std::copy_n(bytes.end() - 16, 16, guarded.after.begin());
        const auto before = guarded.before, after = guarded.after;
        if (guarded.value.OwnSlot1CForAnalysis() != Result::Completed) throw std::runtime_error("replay input was not admitted");
        if (guarded.before != before || guarded.after != after) throw std::runtime_error("replay source-object canary changed");
        const auto& state = guarded.value.GetCapturedStateForAnalysis();
        std::ostringstream output;
        output << Hex(guarded.before.data(), 16) << Hex(state.data(), state.size()) << Hex(guarded.after.data(), 16);
        output << std::hex << std::uppercase << std::setfill('0');
        for (const auto offset : {0x58U, 0x5CU, 0x8CU, 0x90U}) output << ' ' << std::setw(8) << Word(state, offset);
        return output.str();
    }
    std::string CopyReplay(const std::string& line)
    {
        if (line.size() != 768) throw std::runtime_error("copy replay requires exactly384 guarded input bytes");
        std::array<std::uint8_t, 384> bytes;
        for (std::size_t index = 0; index != bytes.size(); ++index)
            bytes[index] = static_cast<std::uint8_t>((Nibble(line[index * 2]) << 4) | Nibble(line[index * 2 + 1]));
        Raw initial; std::copy_n(bytes.begin() + 16, 228, initial.begin());
        std::array<std::uint8_t, 124> sourceEnvelope; std::copy_n(bytes.begin() + 260, 124, sourceEnvelope.begin());
        CopyInput input; std::copy_n(sourceEnvelope.begin() + 16, 92, input.begin());
        Guarded guarded(initial, Profile::PcCapturedRawOwn24);
        std::copy_n(bytes.begin(), 16, guarded.before.begin());
        std::copy_n(bytes.begin() + 244, 16, guarded.after.begin());
        const auto before = guarded.before, after = guarded.after;
        const auto sourceBefore = sourceEnvelope;
        if (guarded.value.OwnSlot24ForAnalysis(input) != Result::Completed) throw std::runtime_error("copy replay input was not admitted");
        if (guarded.before != before || guarded.after != after || sourceEnvelope != sourceBefore)
            throw std::runtime_error("copy replay source-object/source-envelope canary changed");
        const auto& state = guarded.value.GetCapturedStateForAnalysis();
        return Hex(guarded.before.data(), 16) + Hex(state.data(), state.size()) + Hex(guarded.after.data(), 16)
            + " " + Hex(sourceEnvelope.data(), sourceEnvelope.size());
    }

    std::string ComposeReplay(const std::string& line)
    {
        if (line.size() != 768) throw std::runtime_error("compose replay requires exactly384 guarded input bytes");
        std::array<std::uint8_t, 384> bytes;
        for (std::size_t index = 0; index != bytes.size(); ++index)
            bytes[index] = static_cast<std::uint8_t>((Nibble(line[index * 2]) << 4) | Nibble(line[index * 2 + 1]));
        Raw initial; std::copy_n(bytes.begin() + 16, 228, initial.begin());
        std::array<std::uint8_t, 124> sourceEnvelope; std::copy_n(bytes.begin() + 260, 124, sourceEnvelope.begin());
        CopyInput input; std::copy_n(sourceEnvelope.begin() + 16, 92, input.begin());
        Guarded guarded(initial, Profile::PcCapturedFiniteOwn1CAndRawOwn24);
        std::copy_n(bytes.begin(), 16, guarded.before.begin());
        std::copy_n(bytes.begin() + 244, 16, guarded.after.begin());
        const auto before = guarded.before, after = guarded.after;
        const auto sourceBefore = sourceEnvelope;
        if (guarded.value.OwnSlot24ForAnalysis(input) != Result::Completed)
            throw std::runtime_error("compose raw-ingress input was not admitted");
        if (guarded.before != before || guarded.after != after || sourceEnvelope != sourceBefore)
            throw std::runtime_error("compose raw-ingress canary/source changed");
        const auto copyState = guarded.value.GetCapturedStateForAnalysis();
        const auto copyProjection = Hex(guarded.before.data(), 16) + Hex(copyState.data(), copyState.size())
            + Hex(guarded.after.data(), 16) + " " + Hex(sourceEnvelope.data(), sourceEnvelope.size());
        // Keep this same complete receiver and caller source. No fixture setter,
        // input rewrite or reconstruction occurs between the two operations.
        if (guarded.value.OwnSlot1CForAnalysis() != Result::Completed)
            throw std::runtime_error("compose finite-update input was not admitted");
        if (guarded.before != before || guarded.after != after || sourceEnvelope != sourceBefore)
            throw std::runtime_error("compose finite-update canary/source changed");
        const auto& state = guarded.value.GetCapturedStateForAnalysis();
        std::ostringstream output;
        output << copyProjection << ' ' << Hex(guarded.before.data(), 16) << Hex(state.data(), state.size())
            << Hex(guarded.after.data(), 16) << ' ' << Hex(sourceEnvelope.data(), sourceEnvelope.size());
        output << std::hex << std::uppercase << std::setfill('0');
        for (const auto offset : {0x58U, 0x5CU, 0x8CU, 0x90U}) output << ' ' << std::setw(8) << Word(state, offset);
        return output.str();
    }

}

int main(const int argc, char** argv)
{
    try
    {
        if (argc == 2 && std::string(argv[1]) == "--compose-replay")
        { std::string line; while (std::getline(std::cin, line)) { if (!line.empty()) std::cout << ComposeReplay(line) << '\n'; } }
        else if (argc == 2 && std::string(argv[1]) == "--copy-replay")
        { std::string line; while (std::getline(std::cin, line)) { if (!line.empty()) std::cout << CopyReplay(line) << '\n'; } }
        else if (argc == 2 && std::string(argv[1]) == "--replay")
        { std::string line; while (std::getline(std::cin, line)) { if (!line.empty()) std::cout << Replay(line) << '\n'; } }
        else
        { UnitTests(); std::cout << "PASS " << checks << '/' << checks << ": captured ContactConstraint own1C; lifecycle/solver open\n"; }
        return 0;
    }
    catch (const std::exception& error)
    { std::cerr << "FAIL: " << error.what() << '\n'; return 1; }
}
