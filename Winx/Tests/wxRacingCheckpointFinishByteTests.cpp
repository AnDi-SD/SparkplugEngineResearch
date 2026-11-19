#include "../Code/wxRacingCheckpoint.h"
#include "../Analysis/PC/wxRacingCheckpointAbi.h"
#include <algorithm>
#include <iomanip>
#include <iostream>
#include <sstream>
#include <string>

using B = wxRacingCheckpoint;
using Prefix = B::CapturedFinishPrefixForAnalysis;
constexpr auto Profile = B::ProfileForAnalysis::PcCapturedPrimaryFinishLowByte140Prefix141;
static_assert(std::tuple_size<B::CapturedPrefixForAnalysis>::value == 316);
static_assert(std::tuple_size<Prefix>::value == 321);
namespace
{
unsigned contracts = 0;
bool Check(bool condition) { ++contracts; return condition; }
template<std::size_t N> bool Decode(const std::string& text, std::array<std::uint8_t, N>& output)
{
    if (text.size() != N * 2) return false;
    auto digit = [](char c) { return c >= '0' && c <= '9' ? c - '0' : c >= 'a' && c <= 'f' ? c - 'a' + 10 : c >= 'A' && c <= 'F' ? c - 'A' + 10 : -1; };
    for (std::size_t i = 0; i < N; ++i) { const int a = digit(text[2*i]), b = digit(text[2*i+1]); if (a < 0 || b < 0) return false; output[i] = std::uint8_t(a*16+b); }
    return true;
}
template<std::size_t N> void Hex(const std::array<std::uint8_t,N>& value)
{
    for (auto byte : value) std::cout << std::hex << std::setfill('0') << std::setw(2) << unsigned(byte);
}
Prefix Seed(unsigned seed)
{
    Prefix before{};
    for (std::size_t i=0; i<before.size(); ++i) before[i]=std::uint8_t(seed + i*37);
    const auto primary = wxRacingCheckpointAbi::CapturedPrimary;
    for (unsigned i=0; i<4; ++i) before[i]=std::uint8_t(primary>>(8*i));
    return before;
}
bool Unit()
{
    for (unsigned value=0; value<256; ++value)
    {
        auto before=Seed(value); auto expected=before; expected[320]=std::uint8_t(value);
        const std::uint32_t word=0xACBDEF00u|value;
        std::uint8_t raw=0xA6; Prefix after{};
        B::ComposedFinishByteForAnalysis composed{};
        if (!Check(B::ReadFinishByteForAnalysis(before,raw,Profile) && raw==before[320])) return false;
        if (!Check(B::WriteFinishLowByteForAnalysis(before,word,after,Profile) && after==expected)) return false;
        auto alias=before;
        if (!Check(B::WriteFinishLowByteForAnalysis(alias,word,alias,Profile) && alias==expected)) return false;
        if (!Check(B::ComposeFinishLowByteForAnalysis(before,word,composed,Profile) && composed.beforeRaw==before[320] && composed.afterSet==expected && composed.afterRaw==value)) return false;
        B::CapturedPrefixForAnalysis rawBefore{},rawAfter{};
        std::copy_n(before.begin(),rawBefore.size(),rawBefore.begin());
        std::copy_n(after.begin(),rawAfter.size(),rawAfter.begin());
        if (!Check(rawBefore==rawAfter)) return false;
    }
    const auto valid=Seed(37); auto invalid=valid; invalid[0]^=1;
    for (auto profile : {B::ProfileForAnalysis::Unqualified, B::ProfileForAnalysis::PcCapturedPrimaryRawDwords128138, static_cast<B::ProfileForAnalysis>(999)})
    {
        auto after=Seed(81); const auto sentinel=after; std::uint8_t raw=0xA6;
        B::ComposedFinishByteForAnalysis composed{}; composed.beforeRaw=0xA6; composed.afterRaw=0xC5; composed.afterSet=sentinel;
        if (!Check(!B::ReadFinishByteForAnalysis(valid,raw,profile) && raw==0xA6)) return false;
        if (!Check(!B::WriteFinishLowByteForAnalysis(valid,0xFFu,after,profile) && after==sentinel)) return false;
        if (!Check(!B::ComposeFinishLowByteForAnalysis(valid,0xFFu,composed,profile) && composed.beforeRaw==0xA6 && composed.afterRaw==0xC5 && composed.afterSet==sentinel)) return false;
    }
    auto after=Seed(81); const auto sentinel=after; std::uint8_t raw=0xA6;
    if (!Check(!B::ReadFinishByteForAnalysis(invalid,raw,Profile) && raw==0xA6)) return false;
    if (!Check(!B::WriteFinishLowByteForAnalysis(invalid,0xFFu,after,Profile) && after==sentinel)) return false;
    B::ComposedFinishByteForAnalysis invalidComposed{}; invalidComposed.beforeRaw=0xA6; invalidComposed.afterRaw=0xC5; invalidComposed.afterSet=sentinel;
    if (!Check(!B::ComposeFinishLowByteForAnalysis(invalid,0xFFu,invalidComposed,Profile) && invalidComposed.beforeRaw==0xA6 && invalidComposed.afterRaw==0xC5 && invalidComposed.afterSet==sentinel)) return false;
    return true;
}
int Replay()
{
    std::string prefixToken,wordToken;
    while (std::cin>>prefixToken)
    {
        if (!(std::cin>>wordToken)) return 2;
        Prefix before{}; std::array<std::uint8_t,4> bytes{};
        if (!Decode(prefixToken,before) || !Decode(wordToken,bytes)) return 2;
        const auto word=std::uint32_t(bytes[0])|(std::uint32_t(bytes[1])<<8)|(std::uint32_t(bytes[2])<<16)|(std::uint32_t(bytes[3])<<24);
        B::ComposedFinishByteForAnalysis result{};
        if (!B::ComposeFinishLowByteForAnalysis(before,word,result,Profile)) return 2;
        std::cout<<std::hex<<std::setfill('0')<<std::setw(2)<<unsigned(result.beforeRaw)<<' '; Hex(result.afterSet);
        std::cout<<' '<<std::setw(2)<<unsigned(result.afterRaw)<<'\n';
    }
    return 0;
}
}
int main(int argc,char**argv)
{
    if (argc==2 && std::string(argv[1])=="--replay") return Replay();
    if (argc!=1 || !Unit()) return 1;
    std::cout<<"RacingCheckpointFinishByte: "<<contracts<<" contracts passed\n";
    return 0;
}
