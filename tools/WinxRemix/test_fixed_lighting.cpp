// Own analytic checks and bounded packet/constant replay of shared Fixed math.
#include "winx_skin_packet.h"
#include "../../Sparkplug/Analysis/PC/spFixedShaderLighting.h"
#include <cstdio>
#include <fstream>
#include <stdexcept>
#include <string>
namespace pc=sparkplug::evidence::pc;
namespace packet=winx_remix::skin_packet;
static unsigned checks;
static void Check(bool okay,const char* reason){++checks;if(!okay)throw std::runtime_error(reason);}
#include "test_fixed_diffuse.h"
static void Tests(){
    pc::FixedDirectionalLightingForAnalysis p;
    p.ambient={.1f,.2f,.3f,.4f};p.materialDiffuse={.6f,.7f,.8f,.75f};p.constantColor={.125f,.25f,.5f,1};
    p.lightCount=2;p.lights[0]={{0,0,-1,99},{.2f,.4f,.6f,10}};p.lights[1]={{0,0,1,-99},{100,100,100,-10}};
    const pc::FixedSkinVector normal{0,0,.5f,.7f},color{.25f,.5f,1,.25f};
    const std::array<pc::FixedSkinVector,8> expected{{{1,1,1,1},{.6f,.7f,.8f,.75f},color,
        {.3f,.6f,.9f,.75f},{.55f,1.1f,1.9f,.75f},{.075f,.3f,.9f,.1875f},p.constantColor,{.3f,.6f,.9f,.75f}}};
    for(unsigned mode=0;mode<8;++mode){p.colorMode=mode;pc::FixedLightingOutputForAnalysis out;
        Check(pc::ShadeFixedDirectionalForAnalysis(normal,color,p,out),"known ColorMode with signed directional lights");
        for(unsigned j=0;j<4;++j)Check(std::fabs(out.color[j]-expected[mode][j])<1e-6,"original color expression and alpha source");
        Check(out.viewNormal==std::array<float,3>{0,0,1},"unit view normal uses xyz, not the fourth component");}
    p.colorMode=4;p.lightCount=0;
    p.view={{{0,1,0,77},{-1,0,0,88},{0,0,2,99}}};pc::FixedLightingOutputForAnalysis out;
    Check(pc::ShadeFixedDirectionalForAnalysis({1,0,1,1},color,p,out),"view scale and rotation in uploaded register order");
    Check(std::fabs(out.viewNormal[1]-1/std::sqrt(5.f))<1e-6&&std::fabs(out.viewNormal[2]-2/std::sqrt(5.f))<1e-6,"view uses linear rows without translation or inverse transpose");
    p.ambient={-1,2,0,0};
    Check(pc::ShadeFixedDirectionalForAnalysis(normal,color,p,out)&&out.color[0]==-.75f&&out.color[1]==2.5f,"unclamped original color retained");
    const auto previous=out;
    Check(!pc::ShadeFixedDirectionalForAnalysis({0,0,0,1},color,p,out)&&out.color==previous.color,"zero direction rejects without replacing output");
    p.lightCount=9;Check(!pc::ShadeFixedDirectionalForAnalysis(normal,color,p,out),"original directional array bound");
    p.lightCount=1;p.lights[0].direction[0]=INFINITY;
    Check(!pc::ShadeFixedDirectionalForAnalysis(normal,color,p,out),"nonfinite light refuses");
    pc::FixedDiffuseLightingForAnalysis local;local.lightCount=1;
    auto& light=local.lights[0];light.type=1;light.position={0,0,2,1};light.attenuation={2,1,0,0};
    const auto saved=out;
    light.type=3;Check(!pc::ShadeFixedDiffuseForAnalysis({0,0,1,1},{0,0,1},color,local,out),"unknown diffuse shader type refuses");
    light.type=1;light.position={0,0,1,1};
    Check(!pc::ShadeFixedDiffuseForAnalysis({0,0,1,1},{0,0,1},color,local,out),"zero homogeneous light direction refuses");
    light.position={0,0,2,1};light.attenuation={0,0,0,0};
    Check(!pc::ShadeFixedDiffuseForAnalysis({0,0,1,1},{0,0,1},color,local,out),"positive point with zero attenuation denominator refuses");
    light.type=2;light.inner=light.outer=.5f;
    Check(!pc::ShadeFixedDiffuseForAnalysis({0,0,1,1},{0,0,1},color,local,out),"zero spotlight cone width refuses");
    Check(out.color==saved.color&&out.viewNormal==saved.viewNormal,"local light refusal preserves caller output");
}
static std::vector<uint8_t> Read(const char* path,size_t bound){
    std::ifstream stream(path,std::ios::binary|std::ios::ate);Check(bool(stream),"input file open");const auto size=stream.tellg();
    Check(size>=0&&uint64_t(size)<=bound,"file size bound");std::vector<uint8_t> bytes(size_t(size),0);stream.seekg(0);
    Check(bool(stream.read(reinterpret_cast<char*>(bytes.data()),size)),"complete file read");return bytes;
}
#include "test_fixed_uv.h"
static void ReplayDiffuse(const char* source,const char* destination){
    // Own DLP1 fixture: header, six float4 inputs, eight 76-byte light records.
    // Explicit fields avoid depending on the C++ parameter structure's padding.
    const auto bytes=Read(source,720);Check(bytes.size()==720,"DLP1 constant layout");
    uint32_t header[4]{};memcpy(header,bytes.data(),16);
    Check(header[0]==0x31504c44&&header[1]==1,"DLP1 magic and version");
    pc::FixedDiffuseLightingForAnalysis parameters;parameters.colorMode=header[2];parameters.lightCount=header[3];
    size_t offset=16;auto take=[&](auto& vector){memcpy(vector.data(),bytes.data()+offset,16);offset+=16;};
    pc::FixedSkinVector position{},normal{},color{};
    take(position);take(normal);take(color);take(parameters.ambient);take(parameters.materialDiffuse);take(parameters.constantColor);
    for(auto& light:parameters.lights){
        memcpy(&light.type,bytes.data()+offset,4);offset+=4;
        take(light.direction);take(light.diffuse);take(light.position);take(light.attenuation);
        memcpy(&light.inner,bytes.data()+offset,4);memcpy(&light.outer,bytes.data()+offset+4,4);offset+=8;
    }
    Check(offset==bytes.size(),"DLP1 records consume whole input");
    pc::FixedLightingOutputForAnalysis output;
    Check(pc::ShadeFixedDiffuseForAnalysis(position,{normal[0],normal[1],normal[2]},color,parameters,output),"shared Fixed diffuse evaluation");
    std::array<float,7> values{};
    std::copy(output.viewNormal.begin(),output.viewNormal.end(),values.begin());
    std::copy(output.color.begin(),output.color.end(),values.begin()+3);
    std::ofstream stream(destination,std::ios::binary);Check(bool(stream),"diffuse output file open");
    stream.write(reinterpret_cast<const char*>(values.data()),sizeof(values));stream.close();Check(bool(stream),"complete diffuse output and close");
    printf("{\"status\":\"PASS\",\"stride\":28,\"gpu\":false}\n");
}
static void Replay(const char* source,const char* constantFile,const char* destination){
    packet::Packet p;Check(packet::Decode(Read(source,packet::MaximumWireBytes),p),"bounded SKP1 decode");
    const auto bytes=Read(constantFile,368);Check(bytes.size()==368,"FLP1 constant layout");
    uint32_t header[4]{};memcpy(header,bytes.data(),16);Check(header[0]==0x31504c46&&header[1]==1,"FLP1 magic and version");
    pc::FixedDirectionalLightingForAnalysis parameters;parameters.colorMode=header[2];parameters.lightCount=header[3];
    size_t offset=16;auto take=[&](auto& vector){memcpy(vector.data(),bytes.data()+offset,16);offset+=16;};
    for(auto& row:parameters.view)take(row);take(parameters.ambient);take(parameters.materialDiffuse);take(parameters.constantColor);
    for(auto& light:parameters.lights){take(light.direction);take(light.diffuse);}Check(offset==bytes.size(),"constant extent");
    std::vector<pc::FixedSkinDeformationForAnalysis> deformed;Check(packet::Deform(p,deformed),"shared Fixed deformation");
    std::vector<float> values;values.reserve(p.vertices.size()*7);
    for(size_t i=0;i<p.vertices.size();++i){const auto color=p.vertices[i].color;
        pc::FixedSkinVector rgba{float((color>>16)&255)/255,float((color>>8)&255)/255,float(color&255)/255,float(color>>24)/255};
        pc::FixedLightingOutputForAnalysis output;
        Check(pc::ShadeFixedDirectionalForAnalysis(deformed[i].normal,rgba,parameters,output),"shared Fixed directional/color evaluation");
        values.insert(values.end(),output.viewNormal.begin(),output.viewNormal.end());values.insert(values.end(),output.color.begin(),output.color.end());}
    std::ofstream stream(destination,std::ios::binary);Check(bool(stream),"output file open");
    stream.write(reinterpret_cast<const char*>(values.data()),values.size()*sizeof(float));stream.close();Check(bool(stream),"complete output and close");
    printf("{\"status\":\"PASS\",\"vertices\":%zu,\"stride\":28,\"gpu\":false}\n",p.vertices.size());
}
int main(int argc,char** argv){try{
    if(argc==4&&std::string(argv[1])=="--uv"){ReplayUv(argv[2],argv[3]);return 0;}
    if(argc==4&&std::string(argv[1])=="--diffuse"){ReplayDiffuse(argv[2],argv[3]);return 0;}
    if(argc==5&&std::string(argv[1])=="--packet"){Replay(argv[2],argv[3],argv[4]);return 0;}
    Check(argc==1,"usage: test_fixed_lighting [--packet packet.skp params.flp output.bin | --diffuse params.dlp output.bin | --uv params.fuv output.bin]");Tests();DiffuseFixtures();UvFixtures();
    printf("{\"status\":\"PASS\",\"checks\":%u,\"gpu\":false}\n",checks);return 0;
}catch(const std::exception& error){fprintf(stderr,"FAIL %s\n",error.what());return 1;}}
