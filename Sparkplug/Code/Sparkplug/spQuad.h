#pragma once
// Actual RTTI class; only resource ownership needed by LensFlare is restored.
// Quad draw/upload and clone are deliberately not exposed as implemented.
#include "Code/SparkBase/spBaseObject.h"
namespace sparkplug::reconstruction {
class spMaterial;class spVertexBuffer;
class spQuad : public spBaseObject {
public:
    static constexpr spClassID ClassID=0x073411BC;
    ~spQuad() override;
    [[nodiscard]] static const spRTTIRecord& StaticRTTI() noexcept;
    [[nodiscard]] const spRTTIRecord& vfunc_18() const noexcept override;
    [[nodiscard]] std::unique_ptr<spBaseObject> vfunc_10(spCloneManager&) const override{return nullptr;}
    bool vfunc_14(spBaseObject&,spCloneManager&) const override{return false;}
    void SetMaterialForAnalysis(std::shared_ptr<spMaterial> material) noexcept{material_=std::move(material);}
    [[nodiscard]] const std::shared_ptr<spMaterial>& GetMaterialForAnalysis() const noexcept{return material_;}
private:
    // Native PC4CD770 releases material+14 before vertex buffer+10.
    std::shared_ptr<spVertexBuffer> vertices_;
    std::shared_ptr<spMaterial> material_;
};
}
