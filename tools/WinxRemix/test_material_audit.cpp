// Own parser boundary tests and reflection output for independent D3DX comparison.
#define WINX_REMIX_TEST
#include "winx_d3d9_probe.cpp"
#include <filesystem>
#include <fstream>
#include <cstdlib>
static unsigned checks;
static void Check(bool v,const char* message) {++checks;if(!v){fprintf(stderr,"FAIL: %s\n",message);std::exit(1);}}
static void Put(std::vector<uint8_t>& b,size_t at,uint32_t v) {memcpy(b.data()+at,&v,4);}
int wmain(int argc,wchar_t** argv) {
  using material_audit::ReflectColors;
  std::vector<uint8_t> fixture(96);Put(fixture,0,0xfffe0101);Put(fixture,4,(21u<<16)|0xfffe);
  Put(fixture,8,0x42415443);Put(fixture,12,28);Put(fixture,20,0xfffe0101);Put(fixture,24,1);Put(fixture,28,28);
  Put(fixture,40,64);Put(fixture,44,(7u<<16)|2);Put(fixture,48,1);Put(fixture,52,48);
  Put(fixture,60,(3u<<16)|1);Put(fixture,64,(4u<<16)|1);Put(fixture,68,1);
  memcpy(fixture.data()+76,"AmbientCol",11);Put(fixture,92,0xffff);
  auto result=ReflectColors(fixture);
  Check(result.valid && result.colors.size()==1 && result.colors[0].name=="AmbientCol" && result.colors[0].first==7,"literal CTAB float4 register");
  for(unsigned mutation=0;mutation<7;++mutation) {
    auto changed=fixture;
    if(mutation==0)Put(changed,4,(32767u<<16)|0xfffe);
    if(mutation==1)Put(changed,24,257);
    if(mutation==2)Put(changed,28,0xffffffff);
    if(mutation==3)Put(changed,40,0xffffffff);
    if(mutation==4)Put(changed,44,(256u<<16)|2);
    if(mutation==5)Put(changed,64,(3u<<16)|1);
    if(mutation==6)Put(changed,0,0xffff0101);
    Check(!ReflectColors(changed).valid,"invalid table rejected");
  }
  Check(!ReflectColors({}).valid,"empty bytecode rejected");
  std::fill(fixture.begin()+76,fixture.begin()+92,uint8_t(0x41));
  Check(!ReflectColors(fixture).valid,"unterminated name rejected");
  std::ostringstream color;material_audit::Color(color,{1,0,.5f,0});
  Check(color.str()=="[1,0,0.5,0]","zero alpha preserved");
  color.str("");material_audit::Color(color,{NAN,INFINITY,-INFINITY,1});
  Check(color.str()=="[null,null,null,1]","nonfinite values stay explicit");
  if(argc!=2) return 2;
  unsigned files=0;
  for(const auto& item:std::filesystem::directory_iterator(argv[1])) {
    if(item.path().extension()!=L".bin")continue;
    std::ifstream input(item.path(),std::ios::binary);
    std::vector<uint8_t> bytes((std::istreambuf_iterator<char>(input)),{});
    const auto reflected=ReflectColors(bytes);++files;
    printf("{\"file\":\"%s\",\"valid\":%s,\"colors\":[",item.path().filename().string().c_str(),reflected.valid?"true":"false");
    bool first=true;for(const auto& value:reflected.colors) {printf("%s[\"%s\",%u,%u]",first?"":",",value.name.c_str(),value.first,value.count);first=false;}
    puts("]}");
  }
  printf("{\"boundaryChecks\":%u,\"files\":%u}\n",checks,files);
  return 0;
}
