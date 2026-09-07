#pragma once
// Inferred path: original class/base names are verified, TU/header not recovered.
#include "../SparkBase/spBaseObject.h"

namespace sparkplug::reconstruction
{
    class spDXVertexDeclaration : public spCrossPlatform
    {
    public:
        static constexpr spClassID ClassID = 0x33C42E58;
        ~spDXVertexDeclaration() override = default;
        [[nodiscard]] static const spRTTIRecord& StaticRTTI() noexcept;
        [[nodiscard]] const spRTTIRecord& vfunc_18() const noexcept override;
        [[nodiscard]] std::unique_ptr<spBaseObject> vfunc_10(spCloneManager&) const override;
        // Native4B23C0 stores converted FVF at14, after inherited name10.
        [[nodiscard]] virtual bool InitializeForAnalysis(std::uint32_t componentFlags);
        [[nodiscard]] std::uint32_t GetFVFCodeForAnalysis() const noexcept { return fvfCode_; }
    protected:
        spDXVertexDeclaration() noexcept = default;
    private:
        std::uint32_t fvfCode_ = 0;
    };
}
