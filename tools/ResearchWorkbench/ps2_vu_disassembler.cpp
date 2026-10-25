// SPDX-FileCopyrightText: 2002-2026 PCSX2 Dev Team
// SPDX-License-Identifier: GPL-3.0-or-later
// Standalone formatting adaptation of PCSX2 DisVU1Micro.cpp. The unmodified
// DisVUops.h/DisVUmicro.h dependency and license are supplied externally.
// Pin and exact hashes: ps2_vu_decoder_source.json. Never link into the engine.
#include <charconv>
#include <cstdint>
#include <cstdio>
#include <cstring>
#include <iostream>
#include <iterator>
#include <sstream>
#include <string>
#include <system_error>

using u32 = std::uint32_t;
static char ostr[1024];
static const char* leafSymbol = "";
static constexpr const char* Commit = "265d123dc33ee0ddf9c31e5bdf6c03f9d64e9d34";
#define ostrA (ostr + std::strlen(ostr))
#define ostrAL (std::size(ostr) - std::strlen(ostr))
#define DisFInterface (u32 code, u32 pc)
#define DisFInterfaceN (code, pc)
using TdisR5900F = char* (*) DisFInterface;
#define MakeDisF(fn, b) char* fn DisFInterface { \
    (void)code; (void)pc; ostr[0] = 0; leafSymbol = #fn; b; return ostr; }
#define _Ft_ ((code >> 16) & 0x1F)
#define _Fs_ ((code >> 11) & 0x1F)
#define _Fd_ ((code >> 6) & 0x1F)
#define _It_ (_Ft_ & 15)
#define _Is_ (_Fs_ & 15)
#define _Id_ (_Fd_ & 15)
#define _X ((code >> 24) & 1)
#define _Y ((code >> 23) & 1)
#define _Z ((code >> 22) & 1)
#define _W ((code >> 21) & 1)
#define _Fsf_ ((code >> 21) & 3)
#define _Ftf_ ((code >> 23) & 3)
enum { REG_STATUS_FLAG=16, REG_MAC_FLAG=17, REG_CLIP_FLAG=18,
       REG_R=20, REG_I=21, REG_Q=22, REG_P=23 };
static const char* IntegerName(unsigned index)
{
    static char name[16];
    switch (index)
    {
    case REG_STATUS_FLAG: return "STATUS";
    case REG_MAC_FLAG: return "MAC";
    case REG_CLIP_FLAG: return "CLIP";
    case REG_R: return "R"; case REG_I: return "I";
    case REG_Q: return "Q"; case REG_P: return "P";
    default: std::snprintf(name, sizeof(name), "vi%02u", index); return name;
    }
}
static int SignedImmediate(u32 value, unsigned width)
{
    const unsigned mask = (1U << width) - 1;
    value &= mask;
    return value & (1U << (width - 1)) ? int(value) - int(1U << width) : int(value);
}
#define dName(i) std::snprintf(ostrA, ostrAL, " %s", i)
#define dNameU(i) std::snprintf(ostrA, ostrAL, " %s.%s%s%s%s", \
    i, _X ? "x" : "", _Y ? "y" : "", _Z ? "z" : "", _W ? "w" : "")
#define dCP2128f(i) std::snprintf(ostrA, ostrAL, " vf%02u,", unsigned(i))
#define dCP232x(i) std::snprintf(ostrA, ostrAL, " vf%02u.x,", unsigned(i))
#define dCP232y(i) std::snprintf(ostrA, ostrAL, " vf%02u.y,", unsigned(i))
#define dCP232z(i) std::snprintf(ostrA, ostrAL, " vf%02u.z,", unsigned(i))
#define dCP232w(i) std::snprintf(ostrA, ostrAL, " vf%02u.w,", unsigned(i))
#define dCP232f(i,j) std::snprintf(ostrA, ostrAL, " vf%02u.%c,", unsigned(i), "xyzw"[j])
#define dCP2ACCf() std::snprintf(ostrA, ostrAL, " ACC,")
#define dCP232i(i) std::snprintf(ostrA, ostrAL, " %s,", IntegerName(i))
#define dCP232iF(i) dCP232i(i)
#define dImm5() std::snprintf(ostrA, ostrAL, " %d,", SignedImmediate(code >> 6, 5))
#define dImm11() std::snprintf(ostrA, ostrAL, " %d,", SignedImmediate(code, 11))
#define dImm15() std::snprintf(ostrA, ostrAL, " %u,", ((code >> 10) & 0x7800) | (code & 0x7FF))
static MakeDisF(disNULL, dName("*** Bad OP ***");)
#include "DisVUmicro.h"
#include "DisVUops.h"
_disVUOpcodes(ExternalVU);
_disVUTables(ExternalVU);

static void RepairKnownDisassemblerTableTypos()
{
    // Same pinned PCSX2 VUops.cpp interpreter tables3705..3748 select SUBA,
    // unlike four legacy debugger table cells selecting SUB. Leave dependency
    // bytes unchanged; expose this narrow repair in the protocol identity.
    disExternalVU_UPPER_FD_00_TABLE[1] = disExternalVUMI_SUBAx;
    disExternalVU_UPPER_FD_01_TABLE[1] = disExternalVUMI_SUBAy;
    disExternalVU_UPPER_FD_10_TABLE[1] = disExternalVUMI_SUBAz;
    disExternalVU_UPPER_FD_11_TABLE[1] = disExternalVUMI_SUBAw;
}

static std::string Escape(const std::string& text)
{
    std::string result;
    for (unsigned char c : text)
    {
        if (c == '"' || c == '\\') { result += '\\'; result += char(c); }
        else if (c < 32) { char hex[7]; std::snprintf(hex, sizeof(hex), "\\u%04x", c); result += hex; }
        else result += char(c);
    }
    return result;
}
static bool HexWord(const std::string& token, u32& value)
{
    if (token.empty() || token.size() > 8) return false;
    const auto parsed = std::from_chars(token.data(), token.data() + token.size(), value, 16);
    return parsed.ec == std::errc{} && parsed.ptr == token.data() + token.size();
}
int main(int argc, char** argv)
{
    RepairKnownDisassemblerTableTypos();
    if (argc == 2 && std::string(argv[1]) == "--identity")
    {
        std::cout << "{\"protocol\":1,\"upstreamCommit\":\"" << Commit
            << "\",\"decoder\":\"PCSX2 external VU micro disassembler\",\"tableRepairs\":4,\"executesVu\":false}\n";
        return 0;
    }
    if (argc != 1) return 2;
    char rawLine[98];
    unsigned records = 0;
    while (std::cin.getline(rawLine, sizeof(rawLine)))
    {
        const std::string line(rawLine);
        if (++records > 2048 || line.size() > 96) return 2;
        std::string lowerHex, upperHex, pcHex, extra;
        std::istringstream input(line);
        u32 lower = 0, upper = 0, pc = 0;
        if (!(input >> lowerHex >> upperHex >> pcHex) || (input >> extra)
            || !HexWord(lowerHex, lower) || !HexWord(upperHex, upper) || !HexWord(pcHex, pc)) return 2;
        const std::string upperText = disExternalVUMicroUF(upper, pc);
        const std::string upperSymbol = leafSymbol;
        std::string lowerText, lowerSymbol;
        if (upper & 0x80000000U)
        {
            char immediate[32];
            std::snprintf(immediate, sizeof(immediate), " LOI I, 0x%08x", lower);
            lowerText = immediate; lowerSymbol = "LOI";
        }
        else
        {
            lowerText = disExternalVUMicroLF(lower, pc);
            lowerSymbol = leafSymbol;
        }
        std::cout << "{\"pc\":" << pc << ",\"lowerWord\":" << lower << ",\"upperWord\":" << upper
            << ",\"upperText\":\"" << Escape(upperText) << "\",\"upperSymbol\":\"" << upperSymbol
            << "\",\"lowerText\":\"" << Escape(lowerText) << "\",\"lowerSymbol\":\"" << lowerSymbol << "\"}\n";
    }
    return std::cin.eof() ? 0 : 2;
}
