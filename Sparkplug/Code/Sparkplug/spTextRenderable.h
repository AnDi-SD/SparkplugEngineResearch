#pragma once
// Inferred path; PC41A640 factory, 4380F0 owning setter, 437EE0 layout.
// std::string/shared_ptr replace native storage, not its byte-string rules.
#include "spRenderable.h"
#include "spFontManager.h"
#include <optional>
#include <string>
namespace sparkplug::reconstruction {
class spFont;
class spFontManager;
class spTextRenderable final : public spRenderable {
public:
    static constexpr spClassID ClassID=0x19A745D7;
    static const spRTTIRecord& StaticRTTI() noexcept;
    const spRTTIRecord& vfunc_18() const noexcept override;
    std::unique_ptr<spBaseObject> vfunc_10(spCloneManager&) const override { return nullptr; }
    bool vfunc_14(spBaseObject&,spCloneManager&) const override { return false; }
    // Host guard rejects null (original setter dereferences it) and unknown
    // nonempty layout dependencies. Empty-string and omitted-string differ.
    bool SetTextForAnalysis(const char*,spFontManager*,std::string* error=nullptr);
    bool SetFontForAnalysis(std::shared_ptr<spFont>,spFontManager*,std::string* error=nullptr);
    bool SetWrapWidthForAnalysis(std::uint32_t,spFontManager*,std::string* error=nullptr);
    bool SetAlignmentForAnalysis(std::uint32_t,spFontManager*,std::string* error=nullptr);
    void SetColorForAnalysis(std::uint32_t color) noexcept { color_=color; }
    bool RebuildLayoutForAnalysis(spFontManager*,std::string* error=nullptr);
    // Geometry part of437D30. Callbacks/alpha dispatch/material save/restore
    // remain external; alignment uses cached measured width exactly as PC.
    bool BuildPCGeometryForAnalysis(spFontManager&,
        spFontManager::Text3DGeometryForAnalysis&,std::string* error=nullptr) const;
    // Material/font selection prefix437D6D..437D9C. Caller owns the manager
    // draw scope and restoration; no alpha gate/callback/frame claim.
    bool SelectPCDrawResourcesForAnalysis(spFontManager&,std::string* error=nullptr) const;
    bool RequiresPCAlphaQueueForAnalysis() const noexcept override { return IsAlphaSortEnabledForAnalysis(); }
    const std::optional<std::string>& GetTextForAnalysis() const noexcept { return text_; }
    const std::shared_ptr<spFont>& GetFontForAnalysis() const noexcept { return font_; }
    std::uint32_t GetColorForAnalysis() const noexcept { return color_; }
    std::uint32_t GetWrapWidthForAnalysis() const noexcept { return wrap_; }
    std::uint32_t GetAlignmentForAnalysis() const noexcept { return alignment_; }
    std::uint32_t GetMeasuredWidthForAnalysis() const noexcept { return measuredWidth_; }
    const BoundingSphere& GetBoundingSphereForAnalysis() const noexcept override { return sphere_; }
    const std::optional<BoundsPosition>& GetMinimumForAnalysis() const noexcept { return minimum_; }
    const std::optional<BoundsPosition>& GetMaximumForAnalysis() const noexcept { return maximum_; }
    void GetBoundsForAnalysis(BoundsPosition&,BoundsPosition&) const noexcept override;
private:
    std::optional<std::string> text_;
    std::shared_ptr<spFont> font_;
    std::uint32_t color_=0xFFFFFFFFu,wrap_=0,alignment_=0,measuredWidth_=0;
    BoundingSphere sphere_{};
    std::optional<BoundsPosition> minimum_,maximum_; // native ctor leaves both unwritten
};
}
