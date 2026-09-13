#pragma once
// Our owned, bounded interchange for the recovered PC Fixed.rfx contract.
// No native pointers, COM objects, resource names or renderer handles are kept.
#include "../../Sparkplug/Analysis/PC/spFixedShaderSkinning.h"
#include "../../Sparkplug/Code/SparkplugPC/spPCVertexDeclarationElements.h"
#include <algorithm>
#include <cstring>
#include <limits>
#include <vector>

namespace winx_remix::skin_packet {
namespace pc=sparkplug::evidence::pc;
using Element=sparkplug::reconstruction::spPCVertexElementForAnalysis;
using Matrix4=std::array<float,16>;
constexpr uint32_t MaximumVertices=65536,MaximumTriangles=32768,MaximumBones=16;
constexpr size_t MaximumSourceBytes=8*1024*1024,MaximumWireBytes=8*1024*1024;
struct Vertex {
  pc::FixedSkinVertexForAnalysis skin{};
  std::array<float,2> uv{};
  uint32_t color=0xffffffff,sourceIndex=0;
};
struct Packet {
  uint32_t influences=0,attributes=0; // bit0: UV0, bit1: authored COLOR0
  std::vector<Vertex> vertices;
  std::vector<uint32_t> indices; // triangle list, original strip winding retained
  std::vector<pc::FixedSkinMatrix> palette; // owned original shader-register order
};
enum class Error {None,Range,Layout,Palette,Index,Nonfinite,Deformation,Wire};
inline bool Fail(Error* error,Error value){if(error)*error=value;return false;}
inline bool Finite(float x){return std::isfinite(x);}
inline bool Validate(const Packet& p,Error* error=nullptr) {
  if(error)*error=Error::None;
  if(!p.influences||p.influences>4||p.attributes>3||p.vertices.empty()||p.vertices.size()>MaximumVertices||
     p.indices.empty()||p.indices.size()%3||p.indices.size()>size_t(MaximumTriangles)*3||
     p.palette.empty()||p.palette.size()>MaximumBones)return Fail(error,Error::Range);
  for(const auto& matrix:p.palette)for(const auto& row:matrix)for(float x:row)
    if(!Finite(x))return Fail(error,Error::Nonfinite);
  for(const auto index:p.indices)if(index>=p.vertices.size())return Fail(error,Error::Index);
  for(const auto& vertex:p.vertices){
    if(vertex.skin.position[3]!=1||vertex.skin.normal[3]!=1||vertex.sourceIndex>=MaximumVertices)
      return Fail(error,Error::Layout);
    if((!(p.attributes&1)&&(vertex.uv[0]!=0||vertex.uv[1]!=0))||
       (!(p.attributes&2)&&vertex.color!=0xffffffff))return Fail(error,Error::Layout);
    for(float x:vertex.skin.position)if(!Finite(x))return Fail(error,Error::Nonfinite);
    for(float x:vertex.skin.normal)if(!Finite(x))return Fail(error,Error::Nonfinite);
    for(float x:vertex.uv)if(!Finite(x))return Fail(error,Error::Nonfinite);
    for(unsigned b=0;b<p.influences;++b){
      if(!Finite(vertex.skin.weights[b]))return Fail(error,Error::Nonfinite);
      if(vertex.skin.indices[b]>=p.palette.size())return Fail(error,Error::Index);
    }
    // Unused lanes are canonical, never copied from undefined source padding.
    for(unsigned b=p.influences;b<4;++b)
      if(vertex.skin.weights[b]!=0||vertex.skin.indices[b])return Fail(error,Error::Layout);
  }
  return true;
}

// Parse only the current draw's referenced source vertices. A normal is required;
// absent UV/color have explicit defaults and remain distinguishable in attributes.
// Source must be stable for this operation. All outputs remain unchanged on failure.
inline bool Build(const std::vector<uint8_t>& bytes,const std::vector<uint8_t>& indexBytes,
    const Element* elements,size_t elementCount,uint32_t stride,uint32_t vertexBegin,uint32_t vertexCount,
    uint32_t indexBegin,uint32_t triangles,uint32_t primitiveType,uint32_t influences,
    const Matrix4* nativePalette,size_t paletteCount,Packet& output,Error* error=nullptr) {
  if(error)*error=Error::None;
  if(bytes.size()>MaximumSourceBytes||indexBytes.size()>MaximumSourceBytes||!stride||stride>4096||
     !vertexCount||vertexCount>MaximumVertices||!triangles||triangles>MaximumTriangles||
     !influences||influences>4||(primitiveType!=2&&primitiveType!=3)||
     (uint64_t(vertexBegin)+vertexCount)*stride>bytes.size())return Fail(error,Error::Range);
  const uint64_t indexCount=primitiveType==2?uint64_t(triangles)*3:uint64_t(triangles)+2;
  if((uint64_t(indexBegin)+indexCount)*2>indexBytes.size())return Fail(error,Error::Range);
  if(!nativePalette||!paletteCount||paletteCount>MaximumBones)return Fail(error,Error::Palette);
  if(!elements||elementCount<2||elementCount>65||elements[elementCount-1].stream!=0xff||
     elements[elementCount-1].type!=17)return Fail(error,Error::Layout);
  int position=-1,normal=-1,weights=-1,bones=-1,uv=-1,color=-1;
  for(size_t i=0;i+1<elementCount;++i){const auto& e=elements[i];
    const unsigned size=e.type<=3?(unsigned(e.type)+1)*4:e.type==4?4:0;
    if(e.stream||e.method||!size||e.offset>stride||size>stride-e.offset)return Fail(error,Error::Layout);
    for(size_t j=0;j<i;++j){const auto& previous=elements[j];
      const unsigned previousSize=previous.type<=3?(unsigned(previous.type)+1)*4:4;
      if(e.offset<previous.offset+previousSize&&previous.offset<e.offset+size)return Fail(error,Error::Layout);
    }
    int* destination=nullptr;unsigned expected=0;
    if(e.usage==0&&e.usageIndex==0){destination=&position;expected=2;}
    else if(e.usage==3&&e.usageIndex==0){destination=&normal;expected=2;}
    else if(e.usage==1&&e.usageIndex==0){destination=&weights;expected=influences==1?0:3;}
    else if(e.usage==2&&e.usageIndex==0){destination=&bones;expected=3;}
    else if(e.usage==5&&e.usageIndex==0){destination=&uv;expected=1;}
    else if(e.usage==10&&e.usageIndex==0){destination=&color;expected=4;}
    if(destination){if(*destination>=0||e.type!=expected)return Fail(error,Error::Layout);*destination=e.offset;}
  }
  if(position<0||normal<0||weights<0||bones<0)return Fail(error,Error::Layout);
  try {
    Packet packet;packet.influences=influences;packet.attributes=(uv>=0?1u:0u)|(color>=0?2u:0u);
    packet.palette.resize(paletteCount);
    for(size_t b=0;b<paletteCount;++b){const auto& m=nativePalette[b];
      for(float value:m)if(!Finite(value))return Fail(error,Error::Nonfinite);
      if(m[3]!=0||m[7]!=0||m[11]!=0||m[15]!=1)return Fail(error,Error::Palette);
      for(unsigned row=0;row<3;++row)for(unsigned col=0;col<4;++col)packet.palette[b][row][col]=m[col*4+row];
    }
    std::vector<uint32_t> remap(vertexCount,UINT32_MAX);
    packet.indices.reserve(size_t(triangles)*3);packet.vertices.reserve((std::min)(vertexCount,triangles*3));
    for(uint32_t triangle=0;triangle<triangles;++triangle)for(uint32_t corner=0;corner<3;++corner){
      const uint32_t winding=primitiveType==3&&(triangle&1)&&corner<2?1-corner:corner;
      const uint64_t ordinal=uint64_t(indexBegin)+(primitiveType==2?triangle*3:triangle)+winding;
      uint16_t index=0;memcpy(&index,indexBytes.data()+ordinal*2,2);
      if(index>=vertexCount)return Fail(error,Error::Index);
      if(remap[index]==UINT32_MAX){
        const auto row=bytes.data()+(uint64_t(vertexBegin)+index)*stride;Vertex vertex;
        memcpy(vertex.skin.position.data(),row+position,12);vertex.skin.position[3]=1;
        memcpy(vertex.skin.normal.data(),row+normal,12);vertex.skin.normal[3]=1;
        memcpy(vertex.skin.weights.data(),row+weights,influences*4);
        float sourceBones[4]{};memcpy(sourceBones,row+bones,16);
        for(unsigned b=0;b<influences;++b){const float value=sourceBones[b];
          if(!Finite(value)||value<0||value>=paletteCount||std::floor(value)!=value)return Fail(error,Error::Index);
          vertex.skin.indices[b]=uint32_t(value);
        }
        if(uv>=0)memcpy(vertex.uv.data(),row+uv,8);
        if(color>=0)memcpy(&vertex.color,row+color,4);
        vertex.sourceIndex=index;remap[index]=uint32_t(packet.vertices.size());packet.vertices.push_back(vertex);
      }
      packet.indices.push_back(remap[index]);
    }
    if(!Validate(packet,error))return false;
    output=std::move(packet);return true;
  }catch(const std::bad_alloc&){return Fail(error,Error::Range);}
}

inline bool Deform(const Packet& packet,std::vector<pc::FixedSkinDeformationForAnalysis>& output,Error* error=nullptr){
  if(!Validate(packet,error))return false;
  try {std::vector<pc::FixedSkinDeformationForAnalysis> result(packet.vertices.size());
    for(size_t i=0;i<result.size();++i){
      if(!pc::DeformFixedSkinVertexForAnalysis(packet.vertices[i].skin,packet.influences,
         packet.palette.data(),packet.palette.size(),result[i]))return Fail(error,Error::Deformation);
      const auto& n=result[i].normal;
      if(!(double(n[0])*n[0]+double(n[1])*n[1]+double(n[2])*n[2]>0))return Fail(error,Error::Deformation);
    }
    output=std::move(result);return true;
  }catch(const std::bad_alloc&){return Fail(error,Error::Range);}
}

// Explicit little-endian wire, no struct padding or process pointers. One file
// holds one packet. Readers require exact length; partial/trailing records fail.
inline bool Encode(const Packet& packet,std::vector<uint8_t>& output,Error* error=nullptr){
  if(!Validate(packet,error))return false;
  try {std::vector<uint8_t> bytes;
    const size_t size=32+packet.palette.size()*48+packet.vertices.size()*80+packet.indices.size()*4;
    if(size>MaximumWireBytes)return Fail(error,Error::Wire);bytes.reserve(size);
    const auto word=[&](uint32_t value){for(unsigned b=0;b<4;++b)bytes.push_back(uint8_t(value>>(b*8)));};
    const auto real=[&](float value){uint32_t bits=0;memcpy(&bits,&value,4);word(bits);};
    for(uint32_t value:{0x31504b53u,1u,uint32_t(size),uint32_t(packet.vertices.size()),uint32_t(packet.indices.size()),
        packet.influences,uint32_t(packet.palette.size()),packet.attributes})word(value);
    for(const auto& m:packet.palette)for(const auto& row:m)for(float v:row)real(v);
    for(const auto& v:packet.vertices){
      for(float x:v.skin.position)real(x);for(float x:v.skin.normal)real(x);
      for(float x:v.skin.weights)real(x);for(uint32_t x:v.skin.indices)word(x);
      for(float x:v.uv)real(x);word(v.color);word(v.sourceIndex);
    }
    for(uint32_t index:packet.indices)word(index);
    output=std::move(bytes);return true;
  }catch(const std::bad_alloc&){return Fail(error,Error::Wire);}
}
inline bool Decode(const std::vector<uint8_t>& bytes,Packet& output,Error* error=nullptr){
  if(error)*error=Error::None;
  if(bytes.size()<32||bytes.size()>MaximumWireBytes)return Fail(error,Error::Wire);
  size_t offset=0;
  const auto word=[&](){uint32_t value=0;for(unsigned b=0;b<4;++b)value|=uint32_t(bytes[offset++])<<(b*8);return value;};
  const auto real=[&](){const auto bits=word();float value=0;memcpy(&value,&bits,4);return value;};
  if(word()!=0x31504b53u||word()!=1||word()!=bytes.size())return Fail(error,Error::Wire);
  const auto vertices=word(),indices=word(),influences=word(),bones=word(),attributes=word();
  if(!vertices||vertices>MaximumVertices||!indices||indices%3||indices>MaximumTriangles*3||
     !bones||bones>MaximumBones||!influences||influences>4||attributes>3||
     32+uint64_t(bones)*48+uint64_t(vertices)*80+uint64_t(indices)*4!=bytes.size())return Fail(error,Error::Wire);
  try {Packet packet;packet.influences=influences;packet.attributes=attributes;
    packet.palette.resize(bones);packet.vertices.resize(vertices);packet.indices.resize(indices);
    for(auto& m:packet.palette)for(auto& row:m)for(float& x:row)x=real();
    for(auto& v:packet.vertices){
      for(float& x:v.skin.position)x=real();for(float& x:v.skin.normal)x=real();
      for(float& x:v.skin.weights)x=real();for(uint32_t& x:v.skin.indices)x=word();
      for(float& x:v.uv)x=real();v.color=word();v.sourceIndex=word();
    }
    for(auto& index:packet.indices)index=word();
    if(!Validate(packet,error))return false;
    output=std::move(packet);return true;
  }catch(const std::bad_alloc&){return Fail(error,Error::Wire);}
}
} // namespace winx_remix::skin_packet
