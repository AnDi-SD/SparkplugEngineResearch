#pragma once
// Own bounded texture bytes at the native/API boundary. The adapter qualifies
// managed 2D pool, usage, lifetime and current content before calling Capture.
// This class neither calls COM nor assigns hashes, native permission or asset IDs.
#include <cstddef>
#include <cstdint>
#include <utility>
#include <vector>

namespace winx_remix {
class TextureMipSnapshotSource final {
public:
  // Numeric packed 32-bit formats; adapter assertions tie these to its SDK.
  static constexpr std::uint32_t kFormatArgb = 21, kFormatXrgb = 22;
  static constexpr std::uint32_t kDimensionLimit = 2048, kLevelLimit = 13;
  static constexpr std::size_t kWordLimit = 6 * 1024 * 1024;
  static constexpr std::size_t kDdsHeaderWords = 128 / sizeof(std::uint32_t);

  struct Description {
    std::uint32_t width = 0, height = 0, format = 0, levels = 0;
  };
  struct Mip {
    std::uint32_t width = 0, height = 0;
    // Packed source words in row order, with no row padding. XRGB alpha bits
    // remain raw here; hash, normalization and tint equations belong outside.
    std::vector<std::uint32_t> pixels;
  };
  struct Snapshot {
    Description description;
    std::vector<Mip> mips;
    bool valid = false;
  };

  // Mutable owned values are checked again at a consumer boundary. A true
  // result describes layout only, never native lifetime, content or permission.
  static bool IsValid(const Snapshot& snapshot) {
    if (!snapshot.valid || !Valid(snapshot.description) ||
        snapshot.mips.size() != snapshot.description.levels) return false;
    for (std::uint32_t level = 0; level < snapshot.description.levels; ++level) {
      const auto& mip = snapshot.mips[level];
      if (mip.width != Dimension(snapshot.description.width, level) ||
          mip.height != Dimension(snapshot.description.height, level) ||
          mip.pixels.size() != std::size_t(mip.width) * mip.height) return false;
    }
    return true;
  }

  // reader(level, width, height, vector<uint32_t>&) fills an already-sized owned
  // vector without retaining it, and returns false on any failed native read.
  // Every allocation precedes the corresponding reader call, so a native
  // adapter can Lock/Unlock without allocating while a borrowed row is live.
  // The adapter holds its temporary resource reference across Capture and
  // separately rechecks current after releasing that reference. These fences
  // do not replace native lifetime or content qualification.
  // Failure returns an entirely empty value; exceptions propagate. No state
  // from any successful or failed capture is cached by this class.
  template<class Reader, class Current>
  static Snapshot Capture(Description description, const Reader& reader,
                          const Current& current) {
    if (!current() || !Valid(description)) return {};
    Snapshot output;
    output.description = description;
    output.mips.reserve(description.levels);
    for (std::uint32_t level = 0; level < description.levels; ++level) {
      Mip mip;
      mip.width = Dimension(description.width, level);
      mip.height = Dimension(description.height, level);
      const auto count = std::size_t(mip.width) * mip.height;
      mip.pixels.resize(count);
      if (!reader(level, mip.width, mip.height, mip.pixels) || mip.pixels.size() != count)
        return {};
      output.mips.push_back(std::move(mip));
    }
    if (!current()) return {};
    output.valid = true;
    return output;
  }

private:
  static std::uint32_t Dimension(std::uint32_t top, std::uint32_t level) {
    const auto dimension = top >> level;
    return dimension ? dimension : 1;
  }
  static bool Valid(const Description& description) {
    if (!description.width || !description.height ||
        description.width > kDimensionLimit || description.height > kDimensionLimit ||
        !description.levels || description.levels > kLevelLimit ||
        (description.format != kFormatArgb && description.format != kFormatXrgb)) return false;
    auto largest = description.width > description.height ? description.width : description.height;
    std::uint32_t fullChainLevels = 1;
    while (largest > 1) { largest >>= 1; ++fullChainLevels; }
    if (description.levels > fullChainLevels) return false;
    auto words = kDdsHeaderWords;
    for (std::uint32_t level = 0; level < description.levels; ++level) {
      const auto count = std::size_t(Dimension(description.width, level)) *
                         Dimension(description.height, level);
      if (count > kWordLimit - words) return false;
      words += count;
    }
    return true;
  }
};
} // namespace winx_remix
