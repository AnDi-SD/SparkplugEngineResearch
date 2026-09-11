#include "../ShaderLighting.h"
#include <iostream>
#include <limits>

using namespace sparkplug::reconstruction;
namespace {
unsigned checks=0;
void Check(bool ok,const char* message){++checks;if(!ok)throw std::runtime_error(message);}
template<class F> void Reject(F action,const char* message){bool refused=false;try{action();}catch(const std::runtime_error&){refused=true;}Check(refused,message);}
void Vector(const float* actual,const std::array<float,4>& expected,const char* message)
{for(unsigned i=0;i<4;++i)Check(std::abs(actual[i]-expected[i])<1e-6f,message);}
void Prepare(spDXLight& light,spLight::Type type)
{
    light.SetTypeForAnalysis(type);light.SetColorForAnalysis({.5f,.25f,1,.75f});
    light.SetIntensityForAnalysis(1);light.SetRangeForAnalysis(100);light.SetUsesAttenuationForAnalysis(true);
    light.SetHotspotAngleForAnalysis(.6f);light.SetFalloffAngleForAnalysis(1.2f);
    light.SetPositionForAnalysis({2,3,4});light.MarkLocalTransformDirtyForAnalysis();
    Check(light.UpdateWorldForAnalysis(),"Actual Light world/payload update");
}
}
int main()
{
    try {
        const float view[16]={0,0,-1,0,0,1,0,0,1,0,0,0,10,20,30,1};
        spDXMaterial material;material.SetSpecularPowerForAnalysis(0);
        material.SetAmbientColorForAnalysis({.2f,.4f,.6f,.8f});
        material.SetDiffuseColorForAnalysis({.8f,.6f,.4f,.2f});material.SetSpecularColorForAnalysis({.1f,.2f,.3f,.4f});
        Check(material.SetRenderStateForAnalysis(8,3),"Lighting color mode");
        spDXLight ambient,directional,point,spot;
        Prepare(ambient,spLight::Type::Ambient);Prepare(directional,spLight::Type::Directional);
        Prepare(point,spLight::Type::Point);Prepare(spot,spLight::Type::Spot);
        spLightManager::CacheForAnalysis cache;cache.Add(ambient);cache.Add(directional);cache.Add(point);cache.Add(spot);
        auto output=spvhost::CaptureShaderLighting(cache,material,view,0xff336699);
        Check(output.known==15&&output.count==3&&!output.specular,"Selected cache retains ambient separation and count");
        Vector(output.ambient,{.1f,.1f,.6f,.6f},"Ambient parameter product");
        Vector(output.diffuse,{.8f,.6f,.4f,.2f},"Current shader material diffuse");
        for(unsigned i=0;i<3;++i)Vector(output.lights[i].diffuse,{.4f,.15f,.4f,.15f},"Ordinary diffuse products");
        Check(output.lights[0].type==0&&output.lights[0].known==9,"Directional requests no unknown attenuation");
        Vector(output.lights[0].direction,{1,0,0,0},"Original view-space direction producer");
        Check(output.lights[1].type==1&&output.lights[1].known==21,"Point only requests used position and attenuation");
        Vector(output.lights[1].position,{14,23,28,1},"Original view-space position producer");
        auto projected=spDXRenderer::ReadShaderLightForAnalysis(point);
        Vector(output.lights[1].attenuation,projected.attenuation,"Actual device attenuation payload transport");
        Check(output.lights[2].type==2&&output.lights[2].known==109,"Spot marks scalar cone components only");
        Check(std::abs(output.lights[2].inner-.9553365f)<1e-6f&&std::abs(output.lights[2].outer-.8253356f)<1e-6f,"Original half-angle cosine producer");
        point.SetLightEnabledForAnalysis(false);
        auto disabled=spvhost::CaptureShaderLighting(cache,material,view,0);
        Check(disabled.count==3&&disabled.lights[1].known==21,"Shader constants retain selected disabled lights");
        point.SetDevicePayloadForAnalysis({});
        Reject([&]{spvhost::CaptureShaderLighting(cache,material,view,0);},"Unknown consumed point attenuation cannot become zero");
        material.SetSpecularPowerForAnalysis(4);
        auto specular=spvhost::CaptureShaderLighting(cache,material,view,0);
        Check(specular.specular&&specular.power==4&&specular.lights[1].known==7,"Specular point branch does not request unused attenuation");
        for(unsigned i=0;i<3;++i)Vector(specular.lights[i].specular,{.05f,.05f,.3f,.3f},"Specular parameter products");
        Check(material.SetRenderStateForAnalysis(8,2),"Vertex coloring mode");
        auto unlit=spvhost::CaptureShaderLighting(cache,material,view,0);
        Check(unlit.count==3&&unlit.lights[0].known==0&&unlit.lights[1].known==0&&unlit.lights[2].known==0,"Unused lighting constants stay unmarked");
        spDXMaterial uninitialized;
        Reject([&]{spvhost::CaptureShaderLighting(cache,uninitialized,view,0);},"Unknown material power rejected");
        float invalid[16]{};invalid[3]=std::numeric_limits<float>::infinity();
        Reject([&]{spvhost::CaptureShaderLighting(cache,material,invalid,0);},"Non-finite view rejected");
        std::cout<<"PASS "<<checks<<" shader-lighting projection assertions\n";return 0;
    } catch(const std::exception& error){std::cerr<<"FAIL after "<<checks<<": "<<error.what()<<'\n';return 1;}
}
