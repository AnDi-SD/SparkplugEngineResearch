#pragma once
// Own recognition of the supported Fixed Color4 family, generated from the
// shared recovered source composer and the proven original shader resource.
// Shader comments describe registers; only exact executable bytes establish
// program identity. No model name, mesh hash, COM address or guessed register.
#include <algorithm>
#include <array>
#include <cstdint>
#include <cstring>
#include <set>
#include <string>
#include <vector>

namespace winx_remix::skin_shader {
constexpr size_t MaximumCodeBytes=16384,MaximumCatalogBytes=1024*1024;
constexpr std::array<uint8_t,32> SourceDigest={0xac,0x67,0x85,0x42,0x8b,0xa8,0x51,0xde,0xa8,0x8e,0x3a,0x6c,0xdd,0xc0,0x3f,0xdf,
  0x62,0x0f,0xa0,0x6d,0x82,0x06,0x3d,0xb8,0x31,0x76,0x59,0x81,0x40,0x80,0x3d,0xb1};
struct Constant {
  std::string name;
  uint16_t registerSet=0,first=0,count=0,kind=0,type=0,rows=0,columns=0,elements=0,members=0;
  bool operator==(const Constant& b)const {
    return name==b.name&&registerSet==b.registerSet&&first==b.first&&count==b.count&&kind==b.kind&&type==b.type&&
      rows==b.rows&&columns==b.columns&&elements==b.elements&&members==b.members;
  }
};
struct Program {
  std::array<uint32_t,2> key{};
  std::vector<Constant> constants;
  std::vector<uint8_t> instructions;
  const Constant* Find(const char* name)const {
    for(const auto& value:constants)if(value.name==name)return &value;return nullptr;
  }
};
inline uint32_t Word(const uint8_t* bytes){uint32_t value;std::memcpy(&value,bytes,4);return value;}
inline uint16_t Half(const uint8_t* bytes){uint16_t value;std::memcpy(&value,bytes,2);return value;}
inline bool Family(const std::array<uint32_t,2>& key) {
  const auto bones=key[0]&15,lights=(key[0]>>20)&15;
  return bones>=1&&bones<=4&&lights<=3&&key[1]==0&&key[0]==(bones|0x10u|0x40000u|(lights<<20));
}
inline bool Parse(const uint8_t* bytes,size_t size,Program& output) {
  if(!bytes||size<48||size>MaximumCodeBytes||size%4||Word(bytes)!=0xfffe0101||Word(bytes+size-4)!=0xffff)return false;
  const auto comment=Word(bytes+4);const size_t words=(comment>>16)&0x7fff;
  if((comment&0x8000ffff)!=0xfffe||words<8||words>(size-12)/4||Word(bytes+8)!=0x42415443)return false;
  const auto table=bytes+12;const size_t tableSize=(words-1)*4;
  if(Word(table)!=28||Word(table+8)!=0xfffe0101)return false;
  const auto count=Word(table+12),offset=Word(table+16);
  if(!count||count>64||offset>tableSize||uint64_t(count)*20>tableSize-offset)return false;
  Program result;std::set<std::string> names;
  for(unsigned i=0;i<count;++i) {
    const auto entry=table+offset+i*20;const auto nameOffset=Word(entry),typeOffset=Word(entry+12);
    if(nameOffset>=tableSize||typeOffset>tableSize||tableSize-typeOffset<16)return false;
    const auto end=static_cast<const uint8_t*>(std::memchr(table+nameOffset,0,(std::min)(tableSize-nameOffset,size_t(128))));
    if(!end||end==table+nameOffset)return false;
    Constant value;value.name.assign(reinterpret_cast<const char*>(table+nameOffset),size_t(end-table-nameOffset));
    if(!names.insert(value.name).second)return false;
    value.registerSet=Half(entry+4);value.first=Half(entry+6);value.count=Half(entry+8);
    const auto type=table+typeOffset;
    value.kind=Half(type);value.type=Half(type+2);value.rows=Half(type+4);value.columns=Half(type+6);
    value.elements=Half(type+8);value.members=Half(type+10);
    if(value.registerSet!=2||!value.count||value.first>=256||unsigned(value.first)+value.count>256||
       value.kind>3||value.type!=3||!value.rows||value.rows>4||!value.columns||value.columns>4||
       !value.elements||value.elements>256||value.members||Word(type+12))return false;
    result.constants.push_back(std::move(value));
  }
  std::sort(result.constants.begin(),result.constants.end(),[](const Constant& a,const Constant& b){return a.name<b.name;});
  // The leading CTAB comment may have a different creator/padding length.
  // Every remaining byte, including DEF values, must match the trusted compiler
  // output. No instruction decoding or approximate disassembly equivalence.
  result.instructions.assign(bytes+8+words*4,bytes+size);
  if(result.instructions.size()<8)return false;
  output=std::move(result);return true;
}
inline bool Parse(const std::vector<uint8_t>& bytes,Program& output){return Parse(bytes.data(),bytes.size(),output);}
inline bool Layout(const Program& program) {
  if(!Family(program.key))return false;
  const auto blend=program.Find("BlendMatrices"),vp=program.Find("VPTransform"),diffuse=program.Find("MatDiffuse");
  if(!blend||blend->kind!=3||blend->rows!=4||blend->columns!=3||blend->elements!=16||blend->count!=48||
     !vp||vp->kind!=3||vp->rows!=4||vp->columns!=4||vp->elements!=1||vp->count!=4||
     !diffuse||diffuse->kind!=1||diffuse->rows!=1||diffuse->columns!=4||diffuse->elements!=1||diffuse->count!=1)return false;
  const unsigned lights=(program.key[0]>>20)&15;
  for(const auto& value:program.constants) {
    if(value.name!="BlendMatrices"&&value.name!="VPTransform"&&value.name!="MatDiffuse"&&value.name!="AmbientCol"&&
       value.name!="view_matrix"&&value.name!="LightDir"&&value.name!="LightMatDiff")return false;
  }
  const auto ambient=program.Find("AmbientCol");
  if(!ambient||ambient->kind!=1||ambient->rows!=1||ambient->columns!=4||ambient->elements!=1||ambient->count!=1)return false;
  if(lights) {
    const auto view=program.Find("view_matrix"),direction=program.Find("LightDir"),color=program.Find("LightMatDiff");
    if(!view||view->kind!=3||view->rows!=4||view->columns!=4||view->elements!=1||view->count!=3||
       !direction||direction->kind!=1||direction->rows!=1||direction->columns!=4||direction->elements!=8||direction->count!=lights||
       !color||color->kind!=1||color->rows!=1||color->columns!=4||color->elements!=8||color->count!=lights)return false;
  }else if(program.Find("view_matrix")||program.Find("LightDir")||program.Find("LightMatDiff"))return false;
  return true;
}
struct Catalog {
  std::vector<Program> programs;
  bool Decode(const std::vector<uint8_t>& bytes) {
    if(bytes.size()<44||bytes.size()>MaximumCatalogBytes||Word(bytes.data())!=0x31465357||Word(bytes.data()+4)!=1||
       std::memcmp(bytes.data()+12,SourceDigest.data(),SourceDigest.size()))return false;
    const auto count=Word(bytes.data()+8);if(!count||count>64)return false;
    std::vector<Program> result;std::set<std::array<uint32_t,2>> keys;size_t offset=44;
    for(unsigned i=0;i<count;++i) {
      if(bytes.size()-offset<12)return false;
      const std::array<uint32_t,2> key={Word(bytes.data()+offset),Word(bytes.data()+offset+4)};
      const size_t length=Word(bytes.data()+offset+8);offset+=12;
      if(length>bytes.size()-offset||!keys.insert(key).second)return false;
      Program program;if(!Parse(bytes.data()+offset,length,program))return false;
      program.key=key;if(!Layout(program))return false;offset+=length;result.push_back(std::move(program));
    }
    if(offset!=bytes.size())return false;programs=std::move(result);return true;
  }
  const Program* Match(const std::array<uint32_t,2>& key,const std::vector<uint8_t>& bytes)const {
    if(!Family(key))return nullptr;
    Program candidate;if(!Parse(bytes,candidate))return nullptr;
    for(const auto& expected:programs)if(expected.key==key&&candidate.constants==expected.constants&&candidate.instructions==expected.instructions)return &expected;
    return nullptr;
  }
};
} // namespace winx_remix::skin_shader
