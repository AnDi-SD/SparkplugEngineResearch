// Own parser/identity fixture; no D3D device, original code or renderer calls.
#include "winx_skin_shader_contract.h"
#include <fstream>
#include <iostream>
#include <stdexcept>
namespace shader=winx_remix::skin_shader;
static unsigned checks;
static void Check(bool value,const char* message){++checks;if(!value)throw std::runtime_error(message);}
static std::vector<uint8_t> Read(const char* path) {
  std::ifstream file(path,std::ios::binary|std::ios::ate);if(!file)throw std::runtime_error("missing fixture input");
  const auto size=file.tellg();if(size<0||size>shader::MaximumCatalogBytes)throw std::runtime_error("unbounded fixture input");
  std::vector<uint8_t> bytes(static_cast<size_t>(size));file.seekg(0);
  if(size&&!file.read(reinterpret_cast<char*>(bytes.data()),size))throw std::runtime_error("incomplete fixture read");return bytes;
}
static void Store(std::vector<uint8_t>& bytes,size_t at,uint32_t value){std::memcpy(bytes.data()+at,&value,4);}
static void Run(const std::vector<uint8_t>& bytes) {
  shader::Catalog catalog;Check(catalog.Decode(bytes),"prepared catalog must decode");
  Check(catalog.programs.size()==16,"all sixteen declared family variants must be present");
  size_t offset=44;std::vector<size_t> entries;
  for(const auto& expected:catalog.programs) {
    entries.push_back(offset);const auto length=shader::Word(bytes.data()+offset+8);offset+=12;
    std::vector<uint8_t> code(bytes.begin()+offset,bytes.begin()+offset+length);offset+=length;
    Check(catalog.Match(expected.key,code)==&expected,"exact compiler program matches its own key");
    Check(expected.Find("BlendMatrices")&&expected.Find("VPTransform")&&expected.Find("MatDiffuse"),"registers are obtained from verified reflection");
    const auto body=8+4*((shader::Word(code.data()+4)>>16)&0x7fff);
    auto altered=code;altered[body]^=1;
    Check(!catalog.Match(expected.key,altered),"one changed executable token is rejected");
    altered=code;altered[code.size()-5]^=1;
    Check(!catalog.Match(expected.key,altered),"one changed final operand is rejected");
    auto wrongKey=expected.key;wrongKey[0]^=0x100;
    Check(!catalog.Match(wrongKey,code),"UV transform variant cannot borrow the no-transform program");
    wrongKey=expected.key;wrongKey[1]=1;
    Check(!catalog.Match(wrongKey,code),"point-light key cannot borrow a directional program");
    for(size_t end=0;end<code.size();end+=17) {
      shader::Program partial;Check(!shader::Parse(code.data(),end,partial),"truncated program never qualifies");
    }
    altered=code;const auto table=12u;const auto count=shader::Word(code.data()+table+12),constants=shader::Word(code.data()+table+16);
    const auto first=table+constants;
    altered[first+6]^=1;
    Check(!catalog.Match(expected.key,altered),"changed metadata register rejects even with identical executable code");
    altered=code;Store(altered,table+12,UINT32_MAX);
    shader::Program malformed;Check(!shader::Parse(altered,malformed),"overflowing constant table count rejected");
    altered=code;Store(altered,first,UINT32_MAX);
    Check(!shader::Parse(altered,malformed),"out-of-range name rejected");
    altered=code;Store(altered,first+12,UINT32_MAX);
    Check(!shader::Parse(altered,malformed),"out-of-range type rejected");
    if(count>1){altered=code;Store(altered,first+20,shader::Word(code.data()+first));
      Check(!shader::Parse(altered,malformed),"duplicate semantic constant names rejected");}
    // Creator text is a diagnostic comment and does not identify the program.
    altered=code;const auto creator=shader::Word(code.data()+table+4);
    Check(creator<body-table&&altered[table+creator],"test creator string is inside leading comment");
    altered[table+creator]^=1;
    Check(catalog.Match(expected.key,altered)==&expected,"changed creator text preserves identical code and register contract");
    // Padding changes in the CTAB comment must also preserve recognition.
    altered=code;altered.insert(altered.begin()+body,4,0);
    Store(altered,4,shader::Word(code.data()+4)+(1u<<16));
    Check(catalog.Match(expected.key,altered)==&expected,"leading comment padding size is irrelevant to execution");
  }
  Check(offset==bytes.size(),"catalog records consume the whole wire input");
  for(size_t end=0;end<44;++end){shader::Catalog rejected;Check(!rejected.Decode(std::vector<uint8_t>(bytes.begin(),bytes.begin()+end)),"short catalog rejected");}
  for(size_t at:{size_t(0),size_t(4),size_t(8),size_t(12),bytes.size()-1}) {
    auto changed=bytes;changed[at]^=0xff;shader::Catalog rejected;
    Check(!rejected.Decode(changed),"invalid header, source implementation or final program rejected");
  }
  auto duplicate=bytes;Store(duplicate,entries[1],shader::Word(bytes.data()+entries[0]));
  Store(duplicate,entries[1]+4,shader::Word(bytes.data()+entries[0]+4));shader::Catalog rejected;
  Check(!rejected.Decode(duplicate),"duplicate keys cannot override an existing contract");
  auto trailing=bytes;trailing.push_back(0);Check(!rejected.Decode(trailing),"trailing catalog data rejected");
}
int main(int argc,char** argv) {
  try {
    if(argc!=2&&argc!=5)throw std::runtime_error("usage: test_skin_shader_contract CATALOG [MASK LIGHTS SHADER]");
    const auto bytes=Read(argv[1]);Run(bytes);
    bool candidate=false;
    if(argc==5) {
      const std::array<uint32_t,2> key={uint32_t(std::stoul(argv[2])),uint32_t(std::stoul(argv[3]))};
      shader::Catalog catalog;Check(catalog.Decode(bytes),"candidate catalog loaded");
      Check(catalog.Match(key,Read(argv[4]))!=nullptr,"actual original shader must match code and complete register metadata");candidate=true;
    }
    std::cout<<"{\"status\":\"PASS\",\"checks\":"<<checks<<",\"programs\":16,\"candidateMatched\":"<<(candidate?"true":"false")<<",\"gpu\":false}\n";return 0;
  }catch(const std::exception& error){std::cerr<<"FAIL "<<error.what()<<'\n';return 1;}
}
