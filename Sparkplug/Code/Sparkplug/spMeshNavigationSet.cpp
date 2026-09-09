#include "spMeshNavigationSet.h"
#include "Analysis/PC/spRenderNodeMath.h"
#include <cmath>
#include <cstring>
namespace sparkplug::reconstruction
{
    namespace
    {
        std::unique_ptr<spBaseObject> Create(){return std::make_unique<spMeshNavigationSet>();}
        const spRTTIRecord Record{spMeshNavigationSet::ClassID,spNavigationSet::ClassID,"spMeshNavigationSet",&spNavigationSet::StaticRTTI(),&Create,nullptr};
        const bool Registered=spRTTIManager::Instance().RegisterDeferredForAnalysis(Record);
    }
    const spRTTIRecord& spMeshNavigationSet::StaticRTTI() noexcept{(void)Registered;return Record;}
    const spRTTIRecord& spMeshNavigationSet::vfunc_18() const noexcept{return Record;}
    bool spMeshNavigationSet::SetMeshForAnalysis(std::shared_ptr<spMeshBV> value)
    {
        const auto* data=value?value->GetDataForAnalysis():nullptr;
        const auto* vertices=data?data->GetVerticesForAnalysis():nullptr;
        if(!vertices||vertices->GetVertexStrideForAnalysis()!=12)return false;
        const auto& bytes=vertices->GetDataForAnalysis();
        if(bytes.size()%12||bytes.size()/12>4096)return false; // bounded original O(V^2) host work
        const auto& scale=GetWorldScaleForAnalysis();
        const auto inverse=evidence::pc::render_node_math::InversePRS(GetWorldPositionForAnalysis(),
            GetWorldOrientationForAnalysis(),{1.F/scale[0],1.F/scale[1],1.F/scale[2]});
        for(const auto component:inverse)if(!std::isfinite(component))return false;
        mesh_=value.get();collision_.SetPrimitiveForAnalysis(std::move(value));
        const auto point=[&](std::size_t index){Vector3 input{},result{};std::memcpy(input.data(),bytes.data()+index*12,12);
            for(std::size_t c=0;c<3;++c)result[c]=static_cast<float>(double(input[0])*inverse[c]+double(input[1])*inverse[4+c]+double(input[2])*inverse[8+c]+inverse[12+c]);
            return result;};
        // Original444010 keeps earlier min/max on replacement; the farthest
        // pair accumulator is fresh, and an empty/coincident input keeps sphere.
        float bestSquared=0;
        for(std::size_t i=0;i<bytes.size()/12;++i)
        {
            const auto a=point(i);
            for(std::size_t c=0;c<3;++c){if(a[c]<minimum_[c])minimum_[c]=a[c];if(a[c]>maximum_[c])maximum_[c]=a[c];}
            for(std::size_t j=i+1;j<bytes.size()/12;++j)
            {
                const auto b=point(j);double distance=0;Vector3 delta{};
                for(std::size_t c=0;c<3;++c){const auto d=double(a[c])-b[c];distance+=d*d;delta[c]=static_cast<float>(d);}
                if(distance>bestSquared)
                {
                    bestSquared=static_cast<float>(distance);
                    for(std::size_t c=0;c<3;++c)sphere_[c]=static_cast<float>((double(a[c])+b[c])*.5);
                    sphere_[3]=static_cast<float>(std::sqrt(double(delta[0])*delta[0]+double(delta[1])*delta[1]+double(delta[2])*delta[2])*.5);
                }
            }
        }
        return true;
    }
}
