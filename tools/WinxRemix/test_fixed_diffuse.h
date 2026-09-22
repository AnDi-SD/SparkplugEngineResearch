#pragma once
// Authored corner cases checked against the original Fixed shader. Expected
// values retain accumulation order, homogeneous normalization and signed spot
// diffuse; they are independent of the implementation under test.
static void DiffuseFixtures(){
    using Light=pc::FixedDiffuseLightForAnalysis;
    Light point;point.type=1;point.position={0,0,2,1};point.diffuse={.2f,.4f,.1f,0};point.attenuation={2,1,0,0};
    Light directional;directional.direction={0,0,-1,0};directional.diffuse={.3f,.1f,.2f,0};
    Light spot;spot.type=2;spot.position={0,0,2,1};spot.direction={0,0,-1,0};spot.diffuse={.2f,.1f,.3f,0};spot.inner=1;spot.outer=.5f;
    auto homogeneousPoint=point;homogeneousPoint.position[3]=2;
    auto quadraticPoint=point;quadraticPoint.attenuation[2]=1000;
    auto homogeneousSpot=spot;homogeneousSpot.position[3]=2;
    struct Case {const char* name;unsigned count;std::array<Light,2> lights;std::array<float,3> normal;pc::FixedSkinVector ambient,expected;};
    const Case cases[]={
        {"point attenuates ambient plus diffuse",1,{point,{}},{0,0,1},{.1f,.2f,.3f,.4f},{.1f,.2f,.133333333f,.75f}},
        {"backfacing point preserves ambient without attenuation",1,{point,{}},{0,0,-1},{.1f,.2f,.3f,.4f},{.1f,.2f,.3f,.75f}},
        {"point normalizes all four position components",1,{homogeneousPoint,{}},{0,0,1},{.1f,.2f,.3f,.4f},{.0804737854f,.160947571f,.123570226f,.75f}},
        {"point ignores quadratic attenuation",1,{quadraticPoint,{}},{0,0,1},{.1f,.2f,.3f,.4f},{.1f,.2f,.133333333f,.75f}},
        {"point attenuates a preceding directional contribution",2,{directional,point},{0,0,1},{.1f,.2f,.3f,.4f},{.2f,.233333333f,.2f,.75f}},
        {"later directional contribution is not attenuated",2,{point,directional},{0,0,1},{.1f,.2f,.3f,.4f},{.4f,.3f,.333333333f,.75f}},
        {"spot inside cone has no distance attenuation",1,{spot,{}},{0,0,1},{.1f,.2f,.3f,.4f},{.3f,.3f,.6f,.75f}},
        {"spot preserves negative normal dot",1,{spot,{}},{0,0,-1},{.4f,.5f,.6f,.4f},{.2f,.4f,.3f,.75f}},
        {"spot cone uses the homogeneous light direction",1,{homogeneousSpot,{}},{0,0,1},{.1f,.2f,.3f,.4f},{.158578644f,.229289322f,.387867966f,.75f}},
    };
    for(const auto& value:cases){
        pc::FixedDiffuseLightingForAnalysis parameters;parameters.lightCount=value.count;parameters.colorMode=3;
        parameters.ambient=value.ambient;parameters.materialDiffuse={0,0,0,.75f};
        for(unsigned i=0;i<value.count;++i)parameters.lights[i]=value.lights[i];
        pc::FixedLightingOutputForAnalysis result;
        Check(pc::ShadeFixedDiffuseForAnalysis({0,0,1,1},value.normal,{0,0,0,0},parameters,result),value.name);
        for(unsigned i=0;i<4;++i)Check(std::fabs(result.color[i]-value.expected[i])<=2e-6f,value.name);
    }
}
