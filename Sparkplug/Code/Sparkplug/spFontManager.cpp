#include "spFontManager.h"
#include "spFont.h"

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
        // The native destructor releases every vector entry, then the two
        // default-font references, before clearing the singleton support base.
        fallbackFont_.reset();
        primaryFont_.reset();
        fonts_.clear();
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

    void spFontManager::SetPrimaryFontForAnalysis(
        std::shared_ptr<spNamedObject> font)
    {
        primaryFont_ = std::move(font);
    }

    void spFontManager::SetFallbackFontForAnalysis(
        std::shared_ptr<spNamedObject> font)
    {
        fallbackFont_ = std::move(font);
    }

    spNamedObject* spFontManager::GetPrimaryFontForAnalysis() const noexcept
    {
        return primaryFont_.get();
    }

    spNamedObject* spFontManager::GetFallbackFontForAnalysis() const noexcept
    {
        return fallbackFont_.get();
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
        initialized_ = true;
        return true;
    }

    void spFontManager::MarkInitializedForAnalysis(const bool value) noexcept
    {
        initialized_ = value;
    }
}
