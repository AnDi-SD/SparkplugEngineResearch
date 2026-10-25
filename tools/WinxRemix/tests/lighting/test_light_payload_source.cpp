// Own CPU contract fixture. Literal PC fields and independent known consumed
// payloads; no native instructions, controller updates, graphics API or GPU.
#include "../../src/lighting/winx_light_payload_source.h"
#include <cstdio>
#include <cstdlib>
#include <initializer_list>
#include <limits>

using Source = winx_remix::LightPayloadSource;
using Record = winx_remix::SceneLightSnapshot::Record;
static unsigned checks;
static void Check(bool condition, const char* message) {
  ++checks;
  if (!condition) { std::fprintf(stderr, "FAIL: %s\n", message); std::exit(1); }
}
static std::uint32_t Bits(float value) {
  std::uint32_t bits; std::memcpy(&bits, &value, 4); return bits;
}
static void Put32(Record& record, std::size_t offset, std::uint32_t value) {
  // Independent little-endian PC byte encoding, not the source's decoder.
  for (unsigned i = 0; i < 4; ++i) record.raw[offset + i] = static_cast<unsigned char>(value >> (i * 8));
}
static void FloatAt(Record& record, std::size_t offset, float value) { Put32(record, offset, Bits(value)); }

struct Fixture { Record record{}; Source::Payload expected{}; };
static Fixture Make(std::uint32_t type) {
  Fixture fixture;
  auto& record = fixture.record;
  record.address = 0x12000;
  std::memset(record.raw, 0x77, sizeof(record.raw));
  Put32(record, 0, 0x006f0c88); Put32(record, 0x3c, 0x10000);
  Put32(record, 0xb0, 0x100); Put32(record, 0xc0, type);
  record.raw[0xd4] = 0; record.raw[0xed] = 1;
  FloatAt(record, 0x74, 3); FloatAt(record, 0x78, -4); FloatAt(record, 0x7c, 5);
  FloatAt(record, 0xa4, 0); FloatAt(record, 0xa8, 0); FloatAt(record, 0xac, 1);
  FloatAt(record, 0xc4, .25f); FloatAt(record, 0xc8, .5f); FloatAt(record, 0xcc, 1); FloatAt(record, 0xd0, .75f);
  FloatAt(record, 0xd8, 2); FloatAt(record, 0xe0, 10); FloatAt(record, 0xe4, .25f); FloatAt(record, 0xe8, .75f);

  // Unconsumed native words deliberately contain NaN markers. Current source
  // qualification compares consumed words only and preserves the rest.
  auto& words = fixture.expected; words.fill(0x7fc01234u);
  words[0] = type == 0 ? 3 : type;
  words[1] = Bits(type == 0 ? .5f : .25f);
  words[2] = Bits(type == 0 ? 1.f : .5f);
  words[3] = Bits(1);
  if (type != 1) { words[16] = Bits(0); words[17] = Bits(0); words[18] = Bits(1); }
  if (type != 0) {
    words[13] = Bits(3); words[14] = Bits(-4); words[15] = Bits(5); words[19] = Bits(10);
    words[21] = Bits(type == 1 ? .5f : 1.f); words[22] = Bits(0); words[23] = Bits(0);
  }
  if (type == 2) { words[20] = Bits(1); words[24] = Bits(.25f); words[25] = Bits(.75f); }
  for (unsigned i = 0; i < 26; ++i) Put32(record, 0xf0 + i * 4, words[i]);
  return fixture;
}
static void RejectedOutputUnchanged(const Source::Result& result, const char* message) {
  Source::Payload output; output.fill(0x5a5a5a5au);
  const auto before = output;
  Check(!result.CopyCachedEquivalentPayload(output) && output == before, message);
}

static const char* StatusName(Source::Status status) {
  switch (status) {
  case Source::Status::Invalid: return "Invalid";
  case Source::Status::Dirty: return "Dirty";
  case Source::Status::CachedMismatch: return "CachedMismatch";
  case Source::Status::CachedEquivalent: return "CachedEquivalent";
  }
  return "Invalid";
}
static bool DecimalAddress(const char* text, std::uint32_t& address) {
  if (!text || !*text) return false;
  address = 0;
  for (const char* p = text; *p; ++p) {
    if (*p < '0' || *p > '9') return false;
    const auto digit = static_cast<std::uint32_t>(*p - '0');
    if (address > (0xffffffffu - digit) / 10u) return false;
    address = address * 10u + digit;
  }
  return true;
}
static int HexDigit(char digit) {
  if (digit >= '0' && digit <= '9') return digit - '0';
  if (digit >= 'a' && digit <= 'f') return digit - 'a' + 10;
  if (digit >= 'A' && digit <= 'F') return digit - 'A' + 10;
  return -1;
}
static bool HexRecord(const char* text, Record& record) {
  if (!text || std::strlen(text) != sizeof(record.raw) * 2) return false;
  for (std::size_t i = 0; i < sizeof(record.raw); ++i) {
    const int first = HexDigit(text[i * 2]), second = HexDigit(text[i * 2 + 1]);
    if (first < 0 || second < 0) return false;
    record.raw[i] = static_cast<unsigned char>((first << 4) | second);
  }
  return true;
}
static void PrintPayload(const Source::Payload& payload) {
  std::putchar('[');
  for (unsigned i = 0; i < 26; ++i) std::printf(i ? ",%u" : "%u", payload[i]);
  std::putchar(']');
}
static int EvaluateRecord(const char* addressText, const char* hex) {
  Record record{};
  if (!DecimalAddress(addressText, record.address) || !HexRecord(hex, record)) {
    std::fprintf(stderr, "Expected decimal uint32 address and exactly 688 hexadecimal characters\n"); return 2;
  }
  const auto result = Source{}.Evaluate(record);
  std::printf("{\"status\":\"%s\",\"type\":%u,\"flags\":%u,\"worldDirtyMask\":%u,\"lightDataDirty\":%s,"
    "\"consumedMask\":%u,\"differenceMask\":%u,\"candidateAvailable\":%s,\"cached\":",
    StatusName(result.status), result.type, result.flags, result.worldDirtyMask, result.lightDataDirty ? "true" : "false",
    result.consumedMask, result.differenceMask, result.CandidateAvailable() ? "true" : "false");
  PrintPayload(result.cached);
  std::printf(",\"candidate\":"); PrintPayload(result.candidate);
  Source::Payload selected{};
  if (result.CopyCachedEquivalentPayload(selected)) { std::printf(",\"selectedPayload\":"); PrintPayload(selected); }
  std::printf(",\"freshRtQualified\":false}\n");
  return 0;
}

int main(int argc, char** argv) {
  if (argc != 1) {
    if (argc == 4 && !std::strcmp(argv[1], "--record")) return EvaluateRecord(argv[2], argv[3]);
    std::fprintf(stderr, "Usage: test_light_payload_source [--record addressDecimal hex0x158]\n"); return 2;
  }
  Source source;
  const std::uint32_t masks[] = {0x7000Fu, 0xE8E00Fu, 0x3FFE00Fu};
  for (std::uint32_t type = 0; type < 3; ++type) {
    const auto fixture = Make(type);
    const auto before = fixture.record;
    const auto result = source.Evaluate(fixture.record);
    Check(result.status == Source::Status::CachedEquivalent && result.type == type && result.CandidateAvailable(),
      "literal directional/point/spot candidate qualifies cached equality");
    Check(result.consumedMask == masks[type] && !result.differenceMask && result.cached == fixture.expected,
      "exact current consumed mask and owned cached words");
    bool equal = true;
    for (unsigned i = 0; i < 26; ++i) if (masks[type] & (1u << i)) equal &= result.candidate[i] == fixture.expected[i];
    Check(equal, "computed consumed words match independently known payload");
    Source::Payload selected{};
    Check(result.CopyCachedEquivalentPayload(selected) && selected == fixture.expected,
      "qualified output preserves every unused native word including NaN markers");
    Check(fixture.record.address == before.address && !std::memcmp(fixture.record.raw, before.raw, sizeof(before.raw)),
      "evaluation never writes native observation or dirty flags");
    Check(source.Evaluate(fixture.record.address, fixture.record.raw).cached == result.cached,
      "raw-array and snapshot-record interfaces agree");
  }
  Check(Source::ConsumedMask(3) == 0 && Source::ConsumedMask(0xffffffffu) == 0, "ambient and unknown types have no qualified consumed mask");

  {
    auto fixture = Make(1);
    FloatAt(fixture.record, 0xd8, 4);
    const auto result = source.Evaluate(fixture.record);
    Check(result.status == Source::Status::CachedMismatch && result.CandidateAvailable() &&
      result.differenceMask == (1u << 21) && result.candidate[21] == Bits(.25f),
      "changed raw intensity exposes diagnostic candidate rather than qualifying stale cache");
    RejectedOutputUnchanged(result, "mismatched candidate cannot authorize payload source switch");
    const auto saved = result;
    FloatAt(fixture.record, 0xd8, 8);
    const auto next = source.Evaluate(fixture.record);
    Check(saved.candidate[21] == Bits(.25f) && saved.cached[21] == Bits(.5f) &&
      next.candidate[21] == Bits(.125f), "result owns prior cache/candidate bytes and next evaluation reads new fields");
  }
  {
    auto fixture = Make(1); FloatAt(fixture.record, 0x74, 90);
    const auto result = source.Evaluate(fixture.record);
    Check(result.status == Source::Status::CachedMismatch && result.differenceMask == (1u << 13) &&
      result.candidate[13] == Bits(90), "changed observed world position remains diagnostic cached mismatch");
    RejectedOutputUnchanged(result, "world mismatch does not silently replace cached payload");
  }
  {
    auto fixture = Make(2); FloatAt(fixture.record, 0xa4, 1);
    const auto result = source.Evaluate(fixture.record);
    Check(result.status == Source::Status::CachedMismatch && result.differenceMask == (1u << 16) &&
      result.candidate[16] == Bits(1) && result.candidate[18] == Bits(1),
      "source preserves raw direction arithmetic without physical normalization");
  }
  {
    auto fixture = Make(0); FloatAt(fixture.record, 0xd8, 8);
    const auto result = source.Evaluate(fixture.record);
    Check(result.status == Source::Status::CachedMismatch && result.differenceMask == (1u << 1) &&
      result.candidate[1] == Bits(1) && result.candidate[2] == Bits(1) && result.candidate[3] == Bits(1),
      "shared directional intensity arithmetic retains RGB upper clamp");
  }
  {
    auto fixture = Make(1); Put32(fixture.record, 0xf0 + 16 * 4, 0x7fc0abcdu);
    const auto result = source.Evaluate(fixture.record); Source::Payload selected{};
    Check(result.status == Source::Status::CachedEquivalent && !result.differenceMask &&
      result.candidate[16] == Bits(0) && result.CopyCachedEquivalentPayload(selected) && selected[16] == 0x7fc0abcdu,
      "point direction uses explicit diagnostic scratch while selected output preserves unused native bytes");
  }

  for (const auto flag : {0x101u, 0x108u, 0x10fu}) {
    auto fixture = Make(1); Put32(fixture.record, 0xb0, flag);
    const auto result = source.Evaluate(fixture.record);
    Check(result.status == Source::Status::Dirty && !result.CandidateAvailable() && !result.consumedMask && !result.differenceMask,
      "pending world/light marker cannot expose a qualified candidate");
    Check(result.flags == flag && result.worldDirtyMask == (flag & 7u) && result.lightDataDirty == bool(flag & 8u),
      "dirty result separately records observed world and light-data markers");
    RejectedOutputUnchanged(result, "dirty result leaves selected output unchanged");
  }
  {
    auto fixture = Make(1); Put32(fixture.record, 0xb0, 0x108);
    FloatAt(fixture.record, 0xa4, std::numeric_limits<float>::quiet_NaN());
    Check(source.Evaluate(fixture.record).status == Source::Status::Dirty, "dirty classification precedes shared finite-input checks");
    Put32(fixture.record, 0, 0);
    Check(source.Evaluate(fixture.record).status == Source::Status::Invalid, "structural invalidity precedes dirty classification");
  }
  {
    auto fixture = Make(1); fixture.record.address = 0;
    Check(source.Evaluate(fixture.record).status == Source::Status::Invalid, "null object address rejected");
    fixture = Make(1); Put32(fixture.record, 0x3c, 0);
    Check(source.Evaluate(fixture.record).status == Source::Status::Invalid, "missing scene ownership rejected");
    fixture = Make(1); Put32(fixture.record, 0xc0, 3);
    Check(source.Evaluate(fixture.record).status == Source::Status::Invalid, "ambient record has no direct payload source");
    fixture = Make(1); fixture.record.raw[0xd4] = 2;
    Check(source.Evaluate(fixture.record).status == Source::Status::Invalid, "unsupported attenuation boolean representation rejected");
    fixture = Make(1); fixture.record.raw[0xed] = 0;
    Check(source.Evaluate(fixture.record).status == Source::Status::Invalid, "disabled light rejected by payload qualification");
    fixture.record.raw[0xed] = 2;
    Check(source.Evaluate(fixture.record).status == Source::Status::Invalid, "enabled byte must be exactly one");
    fixture = Make(1); Put32(fixture.record, 0xb0, 0);
    Check(source.Evaluate(fixture.record).status == Source::Status::Invalid, "inactive hierarchy rejected by payload qualification");
  }

  // Even unconsumed observed inputs retain the shared producer's finite-input
  // guard. Unused cached payload words above are a different boundary.
  struct NonfiniteCase { std::uint32_t type; std::size_t offset; };
  for (const auto bad : {NonfiniteCase{1,0xc4}, NonfiniteCase{1,0x74}, NonfiniteCase{1,0xd8},
                         NonfiniteCase{2,0xa4}, NonfiniteCase{1,0xe0}, NonfiniteCase{1,0xa4},
                         NonfiniteCase{0,0x74}, NonfiniteCase{1,0xd0}, NonfiniteCase{1,0xe4}, NonfiniteCase{0,0xe8}}) {
    auto fixture = Make(bad.type); FloatAt(fixture.record, bad.offset, std::numeric_limits<float>::quiet_NaN());
    const auto result = source.Evaluate(fixture.record);
    Check(result.status == Source::Status::Invalid && !result.CandidateAvailable(),
      "nonfinite observed source input fails shared producer guard even when unconsumed");
    RejectedOutputUnchanged(result, "invalid observed source leaves selected output unchanged");
  }
  {
    auto fixture = Make(1); FloatAt(fixture.record, 0x20, std::numeric_limits<float>::quiet_NaN());
    Check(source.Evaluate(fixture.record).status == Source::Status::CachedEquivalent,
      "cached equality does not inspect or claim local-PRS/world freshness");
    FloatAt(fixture.record, 0xd8, -2);
    const auto result = source.Evaluate(fixture.record);
    Check(result.status == Source::Status::CachedMismatch && result.candidate[21] == Bits(-.5f),
      "finite negative producer input is not replaced by a new physical-light policy");
  }
  {
    std::uint32_t address = 0;
    Check(DecimalAddress("0", address) && address == 0 && DecimalAddress("4294967295", address) && address == 0xffffffffu,
      "record CLI accepts uint32 decimal bounds");
    Check(!DecimalAddress("4294967296", address) && !DecimalAddress("-1", address) && !DecimalAddress("+1", address) &&
      !DecimalAddress(" 1", address) && !DecimalAddress("1x", address) && !DecimalAddress("", address),
      "record CLI rejects overflow and nondecimal address text");
    Record decoded{};
    char hex[sizeof(decoded.raw) * 2 + 1];
    std::memset(hex, 'a', sizeof(hex) - 1); hex[sizeof(hex) - 1] = 0;
    Check(HexRecord(hex, decoded) && decoded.raw[0] == 0xaa && decoded.raw[0x157] == 0xaa,
      "record CLI decodes exact whole byte buffer");
    hex[0] = 'F'; hex[1] = '0';
    Check(HexRecord(hex, decoded) && decoded.raw[0] == 0xf0, "record CLI accepts both hexadecimal cases");
    hex[0] = 'g';
    Check(!HexRecord(hex, decoded), "record CLI rejects nonhexadecimal record bytes");
    hex[0] = 'a'; hex[sizeof(hex) - 2] = 0;
    Check(!HexRecord(hex, decoded), "record CLI rejects incomplete record buffer");
  }

  std::printf("{\"status\":\"PASS\",\"checks\":%u,\"gpu\":false,\"gameExecuted\":false,\"freshRtQualified\":false}\n", checks);
  return 0;
}
