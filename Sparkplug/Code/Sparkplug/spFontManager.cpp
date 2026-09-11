#include "spFontManager.h"
#include "spFont.h"
#include "spMaterial.h"
#include "spMaterialPassLayer.h"
#include "spStdLayer.h"
#include "spMaterialTexture.h"
#include "../SparkplugDX/spDXMaterial.h"

#include <cstring>

namespace sparkplug::reconstruction
{
    namespace
    {
        const spRTTIRecord FontManagerRecord{
            spFontManager::ClassID,
            spCrossPlatform::ClassID,
            "spFontManager",
            &spCrossPlatform::StaticRTTI(),
            nullptr,
            nullptr,
        };

        const bool FontManagerRegistered =
            spRTTIManager::Instance().RegisterDeferredForAnalysis(FontManagerRecord);
    }

    spFontManager* spFontManager::instance_ = nullptr;

    spFontManager::spFontManager() noexcept
    {
        instance_ = this;
    }

    spFontManager::~spFontManager()
    {
        // PC41FB90: registry entries, fallback38, primary34, then storage.
        fonts_.clear();
        fallbackMaterial_.reset();
        primaryMaterial_.reset();
        initialized_ = false;
        instance_ = nullptr;
    }

    const spRTTIRecord& spFontManager::StaticRTTI() noexcept
    {
        (void)FontManagerRegistered;
        return FontManagerRecord;
    }

    spFontManager* spFontManager::GetInstance() noexcept
    {
        return instance_;
    }

    void spFontManager::SelectFontAndColorForAnalysis(spFont* font,std::uint32_t argb) noexcept
    { selectedFont_=font?font:layoutDefault_;selectedColor_=argb; }

    std::uint32_t spFontManager::MeasureTextForAnalysis(const char* text,
        std::uint32_t wrap,std::uint32_t* height) const noexcept
    {
        if(selectedFont_)return selectedFont_->MeasureTextForAnalysis(text,wrap,height);
        if(height)*height=0;
        return 0;
    }

    std::unique_ptr<spBaseObject> spFontManager::vfunc_10(spCloneManager&) const
    {
        // Both common vtables retain the null-clone contract and registration
        // has no factory; only platform leaves are constructible through RTTI.
        return nullptr;
    }

    const spRTTIRecord& spFontManager::vfunc_18() const noexcept
    {
        return FontManagerRecord;
    }

    bool spFontManager::AddFontForAnalysis(std::shared_ptr<spNamedObject> font)
    {
        if (font == nullptr)
        {
            return false;
        }
        // PS2 0x001685C0 appends unconditionally and increments the intrusive
        // count after insertion; duplicate suppression is not observed.
        fonts_.push_back(std::move(font));
        return true;
    }

    spNamedObject* spFontManager::FindFontForAnalysis(
        const std::string_view name) const noexcept
    {
        for (const auto& font : fonts_)
        {
            const char* const candidate = font == nullptr ? nullptr : font->GetName();
            if (candidate != nullptr && std::string_view{candidate} == name)
            {
                return font.get();
            }
        }
        return nullptr;
    }

    void spFontManager::SetPrimaryMaterialForAnalysis(
        std::shared_ptr<spMaterial> material)
    {
        primaryMaterial_ = std::move(material);
    }

    void spFontManager::SetFallbackMaterialForAnalysis(
        std::shared_ptr<spMaterial> material)
    {
        fallbackMaterial_ = std::move(material);
    }

    spMaterial* spFontManager::GetPrimaryMaterialForAnalysis() const noexcept
    {
        return primaryMaterial_.get();
    }

    spMaterial* spFontManager::GetFallbackMaterialForAnalysis() const noexcept
    {
        return fallbackMaterial_.get();
    }
    const std::shared_ptr<spMaterial>& spFontManager::GetCurrentMaterialForAnalysis() const noexcept
    { return primaryMaterial_?primaryMaterial_:fallbackMaterial_; }

    bool spFontManager::BindPCTextAtlasForAnalysis(std::string* error)
    {
        if(error)error->clear();
        const auto& material=GetCurrentMaterialForAnalysis();
        auto* pass=material?dynamic_cast<spMaterialPassLayer*>(material->GetPassForAnalysis(0)):nullptr;
        auto* layer=pass&&pass->GetLayerCountForAnalysis()?dynamic_cast<spStdLayer*>(pass->GetLayerForAnalysis(0).get()):nullptr;
        if(!selectedFont_||!layer||!layer->GetMaterialTextureForAnalysis()) {
            if(error)*error="TEXT_MATERIAL_SHAPE: selected Font and first StdLayer are required";
            return false; // explicit host guard; original dereferences
        }
        layer->GetMaterialTextureForAnalysis()->SetOwnedFallBackTextureForAnalysis(selectedFont_->GetImageOwnerForAnalysis());
        return true;
    }

    std::size_t spFontManager::GetFontCountForAnalysis() const noexcept
    {
        return fonts_.size();
    }

    bool spFontManager::IsInitializedForAnalysis() const noexcept
    {
        return initialized_;
    }

    bool spFontManager::InitializeForAnalysis()
    {
        initialized_ = false;
        return false;
    }
    bool spFontManager::InitializePCMaterialForAnalysis()
    {
        initialized_=false;
        auto material=std::make_shared<spDXMaterial>();fallbackMaterial_=material;
        auto pass=std::make_shared<spMaterialPassLayer>();pass->SetFinalBlendOperationForAnalysis(2);
        auto layer=std::make_unique<spStdLayer>();
        layer->GetMaterialTextureForAnalysis()->SetTextureStateForAnalysis(2,3);
        layer->GetMaterialTextureForAnalysis()->SetTextureStateForAnalysis(1,3);
        if(!pass->SetLayerForAnalysis(pass->GetLayerCountForAnalysis(),std::move(layer)))return false;
        (void)material->SetRenderStateForAnalysis(8,2);(void)material->SetRenderStateForAnalysis(3,0);
        material->SetVertexAlphaByteForAnalysis(1);(void)material->SetRenderStateForAnalysis(9,1);
        material->SetRenderOverrideByteForAnalysis(1);
        if(!material->SetPassForAnalysis(material->GetPassCountForAnalysis(),pass))return false;
        primaryMaterial_=fallbackMaterial_;initialized_=true;return true;
    }

    void spFontManager::MarkInitializedForAnalysis(const bool value) noexcept
    {
        initialized_ = value;
    }
}
