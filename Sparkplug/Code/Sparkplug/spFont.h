#pragma once
// Inferred source path. PC factory462EC0, reader442660, glyph setter442620.
// Data/lifetime and PC462C00 byte-string measurement. Rendering/clone remain open.
#include "../SparkBase/spBaseObject.h"
#include <array>
#include <optional>

namespace sparkplug::reconstruction {
class spTexture;
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
    spTexture* GetImageForAnalysis() const noexcept { return image_.get(); }
    const std::shared_ptr<spTexture>& GetImageOwnerForAnalysis() const noexcept { return image_; }
    // PC462C00: empty/null returns0 WITHOUT assigning optional height output.
    // Width/height accumulation and wrap comparison are unsigned32 operations.
    std::uint32_t MeasureTextForAnalysis(const char* text,std::uint32_t wrapWidth,
        std::uint32_t* height=nullptr) const noexcept;
private:
    friend class spFontSerializer;
    std::uint32_t height_=0;
    std::optional<std::uint32_t> baseline_;
    std::shared_ptr<spTexture> image_;
    std::array<Glyph,GlyphCount> glyphs_{};
};
}
