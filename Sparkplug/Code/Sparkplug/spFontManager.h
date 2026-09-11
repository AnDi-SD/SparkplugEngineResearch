#pragma once

// Inferred header path.  The common class/module placement follows the native
// registration graph and the exact spPCFontManager.cpp path; no original
// common header string is present in either shipped executable.

#include "../SparkBase/spBaseObject.h"

#include <cstddef>
#include <array>
#include <memory>
#include <string>
#include <string_view>
#include <vector>

namespace sparkplug::reconstruction
{
    class spFont;
    class spMaterial;
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
        void SetPrimaryMaterialForAnalysis(std::shared_ptr<spMaterial> material);
        void SetFallbackMaterialForAnalysis(std::shared_ptr<spMaterial> material);

        [[nodiscard]] spMaterial* GetPrimaryMaterialForAnalysis() const noexcept;
        [[nodiscard]] spMaterial* GetFallbackMaterialForAnalysis() const noexcept;
        [[nodiscard]] const std::shared_ptr<spMaterial>& GetCurrentMaterialForAnalysis() const noexcept;
        [[nodiscard]] std::size_t GetFontCountForAnalysis() const noexcept;
        [[nodiscard]] bool IsInitializedForAnalysis() const noexcept;

        // PC41E770/41E730: three distinct borrowed state words28/2C/30.
        // They are NOT the owning material references34/38 above.
        void SetLayoutDefaultFontForAnalysis(spFont* font) noexcept { layoutDefault_=font; }
        void SelectFontAndColorForAnalysis(spFont* font,std::uint32_t argb) noexcept;
        [[nodiscard]] spFont* GetSelectedFontForAnalysis() const noexcept { return selectedFont_; }
        [[nodiscard]] std::uint32_t GetSelectedColorForAnalysis() const noexcept { return selectedColor_; }
        std::uint32_t MeasureTextForAnalysis(const char*,std::uint32_t,std::uint32_t*) const noexcept;

        // PC41E8B0 builds a DXMaterial, one pass and one StdLayer. Power stays
        // undefined. This does not initialize the platform's default Font.
        bool InitializePCMaterialForAnalysis();
        // Full platform initialization needs an explicit backend font producer;
        // unresolved startup must not report synthetic success.
        virtual bool InitializeForAnalysis();
        struct Text3DVertexForAnalysis {
            std::array<float,3> position{};
            std::uint32_t color=0;
            std::array<float,2> uv{};
        };
        struct Text3DGeometryForAnalysis {
            std::vector<Text3DVertexForAnalysis> vertices;
            std::vector<std::uint16_t> indices;
        };
        // CPU producer inside PC41F3C0; backend acquisition/material/draw are
        // external. Byte strings, signed wrap comparison and word rollback.
        // Host limits:65535 input bytes,4096 glyphs,8*(length+1) iterations.
        bool BuildPCText3DGeometryForAnalysis(const std::array<float,3>& position,
            const char* text,std::uint32_t wrap,Text3DGeometryForAnalysis& output,
            std::string* error=nullptr) const;

    protected:
        void MarkInitializedForAnalysis(const bool value) noexcept;

    private:
        static spFontManager* instance_;
        std::vector<std::shared_ptr<spNamedObject>> fonts_;
        std::shared_ptr<spMaterial> primaryMaterial_;
        std::shared_ptr<spMaterial> fallbackMaterial_;
        bool initialized_ = false;
        spFont* layoutDefault_=nullptr;
        spFont* selectedFont_=nullptr;
        std::uint32_t selectedColor_=0xFFFFFFFFu;
    };
}
