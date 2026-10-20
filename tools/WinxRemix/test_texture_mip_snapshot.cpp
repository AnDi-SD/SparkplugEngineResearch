// Own portable ownership/bounds fixture. No COM, D3D, original game, hash,
// files, native permissions or Remix API are executed by this fixture.
#include "winx_texture_mip_snapshot.h"
#include <algorithm>
#include <cstdio>
#include <cstdlib>
#include <limits>
#include <stdexcept>
#include <utility>

using Source = winx_remix::TextureMipSnapshotSource;
static unsigned checks;
static void Check(bool value, const char* name) {
  ++checks;
  if (!value) { std::fprintf(stderr, "FAIL: %s\n", name); std::exit(1); }
}
static void Empty(const Source::Snapshot& value, const char* name) {
  Check(!value.valid && value.mips.empty() && !value.description.width &&
        !value.description.height && !value.description.format && !value.description.levels, name);
}
struct Fixture {
  Source::Description description{5, 3, Source::kFormatArgb, 3};
  std::vector<std::vector<std::uint32_t>> source{
    {0x00112233, 0x44223344, 0x55334455, 0x66445566, 0x77556677,
     0x88667788, 0x99778899, 0xaa8899aa, 0xbb99aabb, 0xccaabbcc,
     0xddbbccdd, 0xeeccddee, 0xffddeeff, 0x01020304, 0x12131415},
    {0x23456789, 0x3456789a}, {0x456789ab}};
  std::vector<unsigned> events;
  unsigned reads = 0, fences = 0, failRead = ~0u, badSize = ~0u;
  unsigned throwRead = ~0u, throwFence = ~0u, failFence = ~0u;
  bool Read(std::uint32_t level, std::uint32_t width, std::uint32_t height,
            std::vector<std::uint32_t>& pixels) {
    events.push_back(10 + level); ++reads;
    Check(level < source.size() && width * height == source[level].size() &&
          pixels.size() == source[level].size(), "reader receives the exact already-owned output shape");
    if (level == throwRead) throw std::runtime_error("owned reader fault");
    if (level == failRead) { pixels[0] = 0xdeadc0de; return false; }
    std::copy(source[level].begin(), source[level].end(), pixels.begin());
    if (level == badSize) pixels.pop_back();
    return true;
  }
  bool Current() {
    events.push_back(100 + fences); ++fences;
    if (fences == throwFence) throw std::runtime_error("owned current fault");
    return fences != failFence;
  }
  Source::Snapshot Capture() {
    return Source::Capture(description,
      [this](std::uint32_t level, std::uint32_t width, std::uint32_t height,
             std::vector<std::uint32_t>& pixels) { return Read(level, width, height, pixels); },
      [this] { return Current(); });
  }
};
static void ExactOwnedContent() {
  Fixture fixture;
  const auto before = fixture.source;
  const auto first = fixture.Capture();
  Check(first.valid && first.description.width == 5 && first.description.height == 3 &&
        first.description.format == 21 && first.description.levels == 3,
        "non-power-of-two description is copied without reinterpretation");
  Check(first.mips.size() == 3 && first.mips[0].width == 5 && first.mips[0].height == 3 &&
        first.mips[1].width == 2 && first.mips[1].height == 1 &&
        first.mips[2].width == 1 && first.mips[2].height == 1,
        "rectangular chain uses the source's floor-halved dimensions");
  for (unsigned level = 0; level < first.mips.size(); ++level)
    Check(first.mips[level].pixels == before[level], "packed raw words copied exactly per mip");
  Check(fixture.source == before && fixture.events == std::vector<unsigned>{100, 10, 11, 12, 101},
        "current fences enclose all reads without changing source pixels");
  fixture.description.format = Source::kFormatXrgb;
  fixture.source[0][0] = 0x0099aabb;
  fixture.source[2][0] = 0x01abcdef;
  const auto second = fixture.Capture();
  Check(second.valid && second.description.format == 22 && second.mips[0].pixels[0] == 0x0099aabb &&
        second.mips[2].pixels[0] == 0x01abcdef,
        "XRGB remains raw including unused alpha bits before hashing or tint");
  fixture.source.clear();
  Check(first.mips[0].pixels == before[0] && first.mips[2].pixels == before[2] &&
        second.mips[0].pixels[0] == 0x0099aabb,
        "captured bytes survive source mutation and retirement");
  auto changed = second;
  changed.mips[0].pixels[0] = 0; changed.description.width = 1;
  Check(second.mips[0].pixels[0] == 0x0099aabb && second.description.width == 5,
        "snapshot copies own their pixel storage and description independently");
  const auto escaped = [] { Fixture scoped; return scoped.Capture(); }();
  Check(escaped.valid && escaped.mips[1].pixels == before[1],
        "snapshot outlives the reader and all source storage");
}
static void RefusalAndFences() {
  { Fixture f; f.failFence = 1; Empty(f.Capture(), "initial stale input publishes an empty snapshot");
    Check(!f.reads && f.fences == 1, "initial current refusal performs no pixel reads"); }
  { Fixture f; f.failFence = 2; Empty(f.Capture(), "late content mutation discards every copied mip");
    Check(f.reads == 3 && f.fences == 2, "late current fence follows the complete owned read"); }
  for (unsigned level = 0; level < 3; ++level) {
    Fixture f; f.failRead = level;
    Empty(f.Capture(), "a partially written failed read exposes no pixels or description");
    Check(f.reads == level + 1 && f.fences == 1, "reader failure stops later native calls");
    Fixture bad; bad.badSize = level;
    Empty(bad.Capture(), "reader cannot publish a shorter or differently shaped mip");
    Check(bad.reads == level + 1 && bad.fences == 1, "shape failure stops at the failed mip");
  }
  { Fixture f; const auto first = f.Capture(); f.failRead = 1;
    Empty(f.Capture(), "failed capture never returns a prior successful snapshot");
    Check(first.valid && first.mips[1].pixels == f.source[1],
          "failed retry cannot change an earlier returned snapshot"); }
}
static void DescriptionBounds() {
  const Source::Description invalid[] = {
    {0, 1, 21, 1}, {1, 0, 21, 1}, {2049, 1, 21, 1}, {1, 2049, 21, 1},
    {std::numeric_limits<std::uint32_t>::max(), 1, 21, 1},
    {1, std::numeric_limits<std::uint32_t>::max(), 21, 1},
    {1, 1, 0, 1}, {1, 1, 20, 1}, {1, 1, 23, 1}, {1, 1, ~0u, 1},
    {1, 1, 21, 0}, {2048, 2048, 21, 14}, {2048, 2048, 21, ~0u},
    {1, 1, 21, 2}, {5, 3, 21, 4}, {2048, 2048, 21, 13}
  };
  for (const auto& description : invalid) {
    unsigned reads = 0, fences = 0;
    const auto output = Source::Capture(description,
      [&reads](std::uint32_t, std::uint32_t, std::uint32_t, std::vector<std::uint32_t>&) {
        ++reads; return true;
      }, [&fences] { ++fences; return true; });
    Empty(output, "invalid dimensions format or chain length publish an empty value");
    Check(!reads && fences == 1, "invalid descriptions are rejected before any reader allocation or native read");
  }
  { Fixture f; f.description.levels = 1; const auto output = f.Capture();
    Check(output.valid && output.mips.size() == 1 && f.reads == 1,
          "an explicitly truncated mip chain is accepted without inventing lower levels"); }
  { unsigned reads = 0; std::size_t words = Source::kDdsHeaderWords;
    const auto output = Source::Capture({2048, 2048, 21, 12},
      [&reads, &words](std::uint32_t, std::uint32_t width, std::uint32_t height,
                        std::vector<std::uint32_t>& pixels) {
        ++reads; words += pixels.size();
        Check(pixels.size() == std::size_t(width) * height,
              "largest permitted chain supplies bounded pre-sized mip storage");
        pixels.front() = 0x89abcdef; pixels.back() = 0x01234567; return true;
      }, [] { return true; });
    Check(output.valid && reads == 12 && output.mips.back().width == 1 &&
          output.mips.back().height == 1 && words == 5592437 && words <= Source::kWordLimit,
          "largest supported full chain remains inside the DDS-inclusive word budget"); }
  { const auto output = Source::Capture({1, 2048, 22, 12},
      [](std::uint32_t, std::uint32_t width, std::uint32_t height,
         std::vector<std::uint32_t>& pixels) {
        Check(width == 1 && pixels.size() == height, "thin mip dimensions clamp at one"); return true;
      }, [] { return true; });
    Check(output.valid && output.mips.size() == 12, "thin rectangular full chain is supported"); }
}
static void ExceptionsAndRetry() {
  for (unsigned level = 0; level < 3; ++level) {
    Fixture f; f.throwRead = level; bool caught = false;
    try { (void)f.Capture(); } catch (const std::runtime_error&) { caught = true; }
    Check(caught && f.reads == level + 1 && f.fences == 1,
          "reader exception propagates without later reads or cached state");
    f.throwRead = ~0u;
    Check(f.Capture().valid, "capture recovers from a reader exception with a fresh owned value");
  }
  for (unsigned fence = 1; fence <= 2; ++fence) {
    Fixture f; f.throwFence = fence; bool caught = false;
    try { (void)f.Capture(); } catch (const std::runtime_error&) { caught = true; }
    Check(caught && f.fences == fence && f.reads == (fence == 1 ? 0u : 3u),
          "current exceptions propagate at their own qualification boundary");
    f.throwFence = ~0u;
    Check(f.Capture().valid, "capture after a current exception has no poisoned cached snapshot");
  }
}
static void ConsumerLayoutValidation() {
  Fixture fixture;
  const auto source = fixture.Capture();
  Check(Source::IsValid(source), "consumer validates a complete owned snapshot without a native read");
  Check(!Source::IsValid({}), "consumer refuses an empty snapshot");
  { auto bad = source; bad.valid = false;
    Check(!Source::IsValid(bad), "consumer requires successful capture status"); }
  { auto bad = source; bad.description.width = 2049;
    Check(!Source::IsValid(bad), "consumer independently bounds the mutable description"); }
  { auto bad = source; bad.description.format = 20;
    Check(!Source::IsValid(bad), "consumer independently rejects unsupported packed formats"); }
  { auto bad = source; bad.description.levels = 4;
    Check(!Source::IsValid(bad), "consumer refuses a chain longer than the declared natural chain"); }
  { auto bad = source; bad.mips.pop_back();
    Check(!Source::IsValid(bad), "consumer refuses a missing mip"); }
  { auto bad = source; bad.mips.push_back(source.mips.back());
    Check(!Source::IsValid(bad), "consumer refuses an undeclared extra mip"); }
  for (unsigned level = 0; level < source.mips.size(); ++level) {
    { auto bad = source; ++bad.mips[level].width;
      Check(!Source::IsValid(bad), "consumer refuses a mip width inconsistent with its declared level"); }
    { auto bad = source; ++bad.mips[level].height;
      Check(!Source::IsValid(bad), "consumer refuses a mip height inconsistent with its declared level"); }
    { auto bad = source; bad.mips[level].pixels.pop_back();
      Check(!Source::IsValid(bad), "consumer refuses a truncated packed mip payload"); }
    { auto bad = source; bad.mips[level].pixels.push_back(0);
      Check(!Source::IsValid(bad), "consumer refuses an oversized packed mip payload"); }
  }
  Check(Source::IsValid(source) && fixture.reads == 3 && fixture.fences == 2,
        "malformed copy validation neither mutates the original nor performs native qualification");
}
static void DescriptionReentry() {
  Source::Description description{2, 1, 21, 2};
  unsigned reads = 0, fences = 0;
  const auto output = Source::Capture(description,
    [&description, &reads](std::uint32_t level, std::uint32_t width, std::uint32_t height,
                           std::vector<std::uint32_t>& pixels) {
      ++reads;
      Check(width == (level ? 1u : 2u) && height == 1 && pixels.size() == width,
            "reader reentry cannot change the captured description or pre-sized shape");
      description = {0, 0, 0, ~0u};
      return true;
    }, [&description, &fences] {
      ++fences; description = {~0u, ~0u, ~0u, ~0u}; return true;
    });
  Check(Source::IsValid(output) && reads == 2 && fences == 2 &&
        output.description.width == 2 && output.description.height == 1 &&
        output.description.format == 21 && output.description.levels == 2,
        "description is owned before callbacks can mutate the caller's record");
}
int main() {
  ExactOwnedContent(); RefusalAndFences(); DescriptionBounds(); ExceptionsAndRetry(); ConsumerLayoutValidation();
  DescriptionReentry();
  std::printf("{\"status\":\"PASS\",\"checks\":%u}\n", checks);
  return 0;
}
