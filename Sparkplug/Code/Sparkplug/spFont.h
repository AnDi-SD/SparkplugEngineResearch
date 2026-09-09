#pragma once
// Inferred source path. PC factory462EC0, reader442660, glyph setter442620.
// This slice restores font data/lifetime; measurement/rendering/clone are not
// implemented. Their host guards must not be mistaken for original behavior.
#include "../SparkBase/spBaseObject.h"
#include <array>
#include <optional>

namespace sparkplug::reconstruction {
class spTextureData;
class spFontSerializer;
class spFont final : public spNamedObject {
public:
    static constexpr spClassID ClassID=0x4693490A;
    static constexpr std::size_t FirstCharacter=0x20, GlyphCount=224;
    struct Glyph {
        std::uint8_t width=0;
        std::array<float,2> uv0{},uv1{};
    };
    spFont() noexcept = default;
    ~spFont() override;
    static const spRTTIRecord& StaticRTTI() noexcept;
    const spRTTIRecord& vfunc_18() const noexcept override;
    std::unique_ptr<spBaseObject> vfunc_10(spCloneManager&) const override { return nullptr; }
    bool vfunc_14(spBaseObject&,spCloneManager&) const override { return false; }
    std::uint32_t GetHeightForAnalysis() const noexcept { return height_; }
    // Native factory leaves baseline uninitialized. An absent assignment has
    // no invented numeric value in the host representation.
    const std::optional<std::uint32_t>& GetBaselineForAnalysis() const noexcept { return baseline_; }
    const std::array<Glyph,GlyphCount>& GetGlyphsForAnalysis() const noexcept { return glyphs_; }
    spTextureData* GetImageForAnalysis() const noexcept { return image_.get(); }
private:
    friend class spFontSerializer;
    std::uint32_t height_=0;
    std::optional<std::uint32_t> baseline_;
    std::shared_ptr<spTextureData> image_;
    std::array<Glyph,GlyphCount> glyphs_{};
};
}
