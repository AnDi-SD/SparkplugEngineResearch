#pragma once

// Inferred header path.  The common class/module placement follows the native
// registration graph and the exact spPCFontManager.cpp path; no original
// common header string is present in either shipped executable.

#include "../SparkBase/spBaseObject.h"

#include <cstddef>
#include <memory>
#include <string_view>
#include <vector>

namespace sparkplug::reconstruction
{
    class spFont;
    class spFontManager : public spCrossPlatform
    {
    public:
        static constexpr spClassID ClassID = 0x1640375E;

        spFontManager() noexcept;
        ~spFontManager() override;

        spFontManager(const spFontManager&) = delete;
        spFontManager& operator=(const spFontManager&) = delete;

        [[nodiscard]] static const spRTTIRecord& StaticRTTI() noexcept;
        [[nodiscard]] static spFontManager* GetInstance() noexcept;

        [[nodiscard]] std::unique_ptr<spBaseObject> vfunc_10(
            spCloneManager& manager) const override;
        [[nodiscard]] const spRTTIRecord& vfunc_18() const noexcept override;

        // Analytical names for the common native operations.  The manager
        // owns intrusive references to spFont instances; spNamedObject is the
        // narrowest reconstructed base that preserves the proven name lookup.
        [[nodiscard]] bool AddFontForAnalysis(
            std::shared_ptr<spNamedObject> font);
        [[nodiscard]] spNamedObject* FindFontForAnalysis(
            std::string_view name) const noexcept;
        void SetPrimaryFontForAnalysis(std::shared_ptr<spNamedObject> font);
        void SetFallbackFontForAnalysis(std::shared_ptr<spNamedObject> font);

        [[nodiscard]] spNamedObject* GetPrimaryFontForAnalysis() const noexcept;
        [[nodiscard]] spNamedObject* GetFallbackFontForAnalysis() const noexcept;
        [[nodiscard]] std::size_t GetFontCountForAnalysis() const noexcept;
        [[nodiscard]] bool IsInitializedForAnalysis() const noexcept;

        // PC41E770/41E730: three distinct borrowed state words28/2C/30.
        // They are NOT the owning primary/fallback references34/38 above.
        void SetLayoutDefaultFontForAnalysis(spFont* font) noexcept { layoutDefault_=font; }
        void SelectFontAndColorForAnalysis(spFont* font,std::uint32_t argb) noexcept;
        [[nodiscard]] spFont* GetSelectedFontForAnalysis() const noexcept { return selectedFont_; }
        [[nodiscard]] std::uint32_t GetSelectedColorForAnalysis() const noexcept { return selectedColor_; }
        std::uint32_t MeasureTextForAnalysis(const char*,std::uint32_t,std::uint32_t*) const noexcept;

        // Native common initialization builds renderer-owned default font
        // objects.  That graph is outside this slice; this seam preserves the
        // verified success/state transition without inventing those classes.
        virtual bool InitializeForAnalysis();

    protected:
        void MarkInitializedForAnalysis(const bool value) noexcept;

    private:
        static spFontManager* instance_;
        std::vector<std::shared_ptr<spNamedObject>> fonts_;
        std::shared_ptr<spNamedObject> primaryFont_;
        std::shared_ptr<spNamedObject> fallbackFont_;
        bool initialized_ = false;
        spFont* layoutDefault_=nullptr;
        spFont* selectedFont_=nullptr;
        std::uint32_t selectedColor_=0xFFFFFFFFu;
    };
}
