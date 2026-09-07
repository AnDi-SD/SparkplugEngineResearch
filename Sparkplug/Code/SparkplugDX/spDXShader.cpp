#include "spDXShader.h"
#include "spDXRenderer.h"
#include "../Sparkplug/spMaterial.h"
#include "Analysis/PC/spColorMath.h"
#include <algorithm>
#include <cmath>
#include <cstring>
namespace sparkplug::reconstruction
{
    namespace
    {
        const spRTTIRecord Record{spDXShader::ClassID,spCrossPlatform::ClassID,"spDXShader",&spCrossPlatform::StaticRTTI(),nullptr,nullptr};
        const bool Registered=spRTTIManager::Instance().RegisterDeferredForAnalysis(Record);
    }
    const spRTTIRecord& spDXShader::StaticRTTI() noexcept{(void)Registered;return Record;}
    const spRTTIRecord& spDXShader::vfunc_18() const noexcept{return Record;}
    std::uint32_t spDXShader::LookupParameterTypeForAnalysis(std::string_view name) noexcept
    {
        constexpr std::array<std::string_view,23> names{"","view_proj_matrix","view_matrix","VPTransform","inv_view_matrix","BlendMatrices","AmbientCol","ConstColor","MatDiffuse","MatSpecular","MatSpecularPwr","LightMatDiff","LightMatSpec","LightPos","LightDir","LightInner","LightOuter","UVTransform","ViewDirLightDir0","LightAmbientColorDir0","LightDiffuseColorDir0","LightSpecularColorDir0","LightAttenuation"};
        for(std::uint32_t i=1;i<names.size();++i)if(name==names[i])return i;
        return 0;
    }
    bool spDXShader::BuildFullyWrittenConstantsForAnalysis(std::vector<std::uint32_t>& output,
        const ConstantInputsForAnalysis& inputs,std::uint32_t rows)const
    {
        if(rows>256)return false; // explicit native scratch extent, not GPU register limit
        std::vector<std::uint32_t> scratch(1024);std::array<bool,1024> written{};
        if(!BuildConstantsForAnalysis(scratch,inputs))return false;
        for(const auto& parameter:parameters_)
        {
            const auto type=parameter.type;if(!type||type>22)continue;
            const std::size_t start=std::size_t(parameter.startRegister)*4,count=parameter.registerCount;
            const auto mark=[&](std::size_t at,std::size_t length)
            {if(at>1024||length>1024-at)return false;std::fill_n(written.begin()+at,length,true);return true;};
            if(type<=4){if(!mark(start,count*4))return false;}
            else if(type==5){if(!mark(start,(count/3)*12))return false;}
            else if(type==17){for(std::size_t i=0;i<(count/3)*3;++i)if(!mark(start+4*i,3))return false;}
            else if(type==10){if(!mark(start,1))return false;}
            else if(type==15||type==16){for(std::size_t i=0;i<count;++i)if(!mark(start+4*i,1))return false;}
            else if((type>=11&&type<=14)||type==22){if(!mark(start,count*4))return false;}
            else if(!mark(start,4))return false;
        }
        if(!std::all_of(written.begin(),written.begin()+rows*4,[](bool v){return v;}))return false;
        output.assign(scratch.begin(),scratch.begin()+rows*4);return true;
    }
    bool spDXShader::AppendParameterForAnalysis(ParameterForAnalysis parameter)
    {
        const auto end=std::find(parameter.name.begin(),parameter.name.end(),'\0');
        if(end==parameter.name.end())return false;
        parameter.type=LookupParameterTypeForAnalysis(std::string_view(parameter.name.data(),end-parameter.name.begin()));
        parameters_.push_back(parameter);scalarWords_[8]+=parameter.registerCount;return true;
    }
    bool spDXShader::BuildConstantsForAnalysis(std::vector<std::uint32_t>& output,const ConstantInputsForAnalysis& inputs) const
    {
        const auto bits=[](float value){std::uint32_t word;std::memcpy(&word,&value,4);return word;};
        const auto matrixValues=[](const RawMatrixForAnalysis& raw,std::array<double,16>& m)
        {
            for(unsigned i=0;i<16;++i){float value;std::memcpy(&value,&raw[i],4);if(!std::isfinite(value))return false;m[i]=value;}
            return true;
        };
        const auto finite=[](const auto& values){for(auto value:values)if(!std::isfinite(value))return false;return true;};
        const auto product=[&](const std::array<float,4>& a,const std::array<float,4>& b,std::size_t start)
        {
            if(!finite(a)||!finite(b))return false;
            for(unsigned i=0;i<4;++i)output[start+i]=bits(a[i]*b[i]);return true;
        };
        for(const auto& p:parameters_)
        {
            if(!p.type||p.type>22)continue; // actual default branch touches nothing
            const std::size_t start=std::size_t(p.startRegister)*4;
            const auto room=[&](std::size_t count){return start<=output.size()&&count<=output.size()-start;};
            if(p.type>=1&&p.type<=3)
            {
                const std::size_t count=std::size_t(p.registerCount)*4;
                if(count>16||!room(count))return false;
                constexpr std::array<unsigned,3> selected{2,0,1};
                const auto* matrix=inputs.rendererMatrices?
                    spDXRenderer::GetCachedMatrixForAnalysis(*inputs.rendererMatrices,selected[p.type-1]):
                    &inputs.cachedMatrices[selected[p.type-1]];
                if(!matrix)return false;
                std::copy_n(matrix->begin(),count,output.begin()+start);
            }
            else if(p.type==4)
            {
                const std::size_t count=std::size_t(p.registerCount)*4;
                if(count>16||!room(count))return false;
                const auto* matrix=inputs.rendererMatrices?
                    spDXRenderer::GetCachedMatrixForAnalysis(*inputs.rendererMatrices,0):&inputs.cachedMatrices[0];
                std::array<double,16> m;if(!matrix||!matrixValues(*matrix,m))return false;
                RawMatrixForAnalysis inverse{};inverse[15]=0x3f800000;
                for(unsigned row=0;row<3;++row)for(unsigned col=0;col<3;++col)inverse[row*4+col]=(*matrix)[col*4+row];
                // Original461D40→461940: rigid transpose, NOT general inverse.
                // Keep native summation order with wider finite intermediates;
                // this is not a bit-identical implementation of all x87 inputs.
                inverse[12]=bits(static_cast<float>(-((m[13]*m[1]+m[12]*m[0])+m[14]*m[2])));
                inverse[13]=bits(static_cast<float>(-((m[14]*m[6]+m[13]*m[5])+m[12]*m[4])));
                inverse[14]=bits(static_cast<float>(-((m[13]*m[9]+m[14]*m[10])+m[12]*m[8])));
                std::copy_n(inverse.begin(),count,output.begin()+start);
            }
            else if(p.type==5||p.type==17)
            {
                const std::size_t count=p.registerCount/3;
                const auto& matrices=p.type==5?inputs.blendMatrices:inputs.uvMatrices;
                if(count>matrices.size()||(p.type==17&&count>8)||!room(count*12))return false;
                for(std::size_t i=0;i<count;++i)
                {
                    const auto& matrix=matrices[i];
                    for(auto word:matrix)if((word&0x7f800000)==0x7f800000)return false; // finite x87 slice, host guard
                    for(unsigned row=0;row<3;++row)for(unsigned col=0;col<(p.type==5?4u:3u);++col)
                        output[start+i*12+row*4+col]=matrix[p.type==5?col*4+row:row*4+col];
                }
            }
            else if(p.type>=7&&p.type<=10)
            {
                if(!room(p.type==10?1:4))return false; // these ignore registerCount, even0
                if(p.type==10)
                {
                    if(!inputs.material||!inputs.material->HasInitializedSpecularPowerForAnalysis())return false;
                    const auto power=inputs.material->GetSpecularPowerForAnalysis();if(!std::isfinite(power))return false;
                    output[start]=bits(power);
                }
                else
                {
                    if(p.type!=7&&!inputs.material)return false;
                    const auto color=p.type==7?PCARGBToRGBAForAnalysis(inputs.constantColor):
                        p.type==8?inputs.material->GetDiffuseColorForAnalysis():inputs.material->GetSpecularColorForAnalysis();
                    for(unsigned i=0;i<4;++i)output[start+i]=bits(color[i]);
                }
            }
            else if(p.type==6||p.type==19||p.type==20||p.type==21)
            {
                if(!room(4))return false;
                if(p.type==6)
                {
                    if(!inputs.lightListPresent)return false;
                    if(!inputs.ambientLight){std::fill_n(output.begin()+start,4,0);continue;}
                    if(!inputs.material||!product(inputs.ambientLight->color,inputs.material->GetAmbientColorForAnalysis(),start))return false;
                }
                else if(p.type==19)
                {if(!inputs.material||!product(inputs.rendererAmbient,inputs.material->GetAmbientColorForAnalysis(),start))return false;}
                else if(!inputs.directionalLight)
                {std::fill_n(output.begin()+start,3,0);output[start+3]=0x3f800000;}
                else if(p.type==20)
                {if(!inputs.material||!product(inputs.directionalLight->color,inputs.material->GetDiffuseColorForAnalysis(),start))return false;}
                else
                {
                    if(!inputs.material)return false;
                    // Native type21 returns material specular UNMULTIPLIED.
                    const auto& color=inputs.material->GetSpecularColorForAnalysis();for(unsigned i=0;i<4;++i)output[start+i]=bits(color[i]);
                }
            }
            else if(p.type==11||p.type==12||p.type==22)
            {
                const std::size_t count=p.registerCount;
                if(!count)continue;
                if(!inputs.lightListPresent||count>inputs.lights.size()||!room(count*4))return false;
                for(std::size_t i=0;i<count;++i)
                {
                    if(p.type==22)
                    {
                        if(!finite(inputs.lights[i].attenuation))return false;
                        for(unsigned k=0;k<4;++k)output[start+i*4+k]=bits(inputs.lights[i].attenuation[k]);
                    }
                    else
                    {
                        if(!inputs.material)return false;
                        const auto& color=p.type==11?inputs.material->GetDiffuseColorForAnalysis():inputs.material->GetSpecularColorForAnalysis();
                        if(!product(inputs.lights[i].color,color,start+i*4))return false;
                    }
                }
            }
            else if(p.type==13||p.type==14||p.type==15||p.type==16||p.type==18)
            {
                const std::size_t count=p.type==18?1:p.registerCount;
                if(!count)continue;
                if(!room(count*4))return false;
                if(p.type!=18&&(!inputs.lightListPresent||count>inputs.lights.size()))return false;
                if(p.type==18&&!inputs.directionalLight)
                {std::fill_n(output.begin()+start,3,0);output[start+3]=0x3f800000;continue;}
                std::array<double,16> m;
                if(p.type!=15&&p.type!=16&&!matrixValues(inputs.viewMatrix,m))return false;
                for(std::size_t i=0;i<count;++i)
                {
                    const auto& light=p.type==18?*inputs.directionalLight:inputs.lights[i];const auto at=start+i*4;
                    if(p.type==15||p.type==16)
                    {
                        const auto angle=p.type==15?light.innerAngle:light.outerAngle;
                        if(!std::isfinite(angle)||std::fabs(angle)>1024)return false; // finite tested host range
                        output[at]=bits(static_cast<float>(std::cos(double(angle)*.5)));continue;
                    }
                    const auto& vector=p.type==18?light.localDirection:p.type==13&&light.type!=0?light.worldPosition:light.worldDirection;
                    if(!finite(vector))return false;const double x=vector[0],y=vector[1],z=vector[2];
                    if(p.type==14)
                    {
                        output[at]=bits(static_cast<float>((y*m[4]+z*m[8])+x*m[0]));
                        output[at+1]=bits(static_cast<float>((x*m[1]+y*m[5])+z*m[9]));
                        output[at+2]=bits(static_cast<float>((x*m[2]+y*m[6])+z*m[10]));output[at+3]=0;
                    }
                    else
                    {
                        const bool position=p.type==13&&light.type!=0;
                        std::array<float,3> transformed{
                            static_cast<float>(((z*m[8]+y*m[4])+x*m[0])+(position?m[12]:0)),
                            static_cast<float>(((z*m[9]+x*m[1])+y*m[5])+(position?m[13]:0)),
                            static_cast<float>(((z*m[10]+x*m[2])+y*m[6])+(position?m[14]:0))};
                        if(p.type==13&&!position)for(auto& v:transformed)v=-v*1000000000.0f;
                        for(unsigned k=0;k<3;++k)output[at+k]=bits(transformed[k]);
                        output[at+3]=p.type==18?bits(static_cast<float>(((z*m[11]+x*m[3])+y*m[7])+m[15])):0x3f800000;
                    }
                }
            }
            else return false;
        }
        return true;
    }
}
