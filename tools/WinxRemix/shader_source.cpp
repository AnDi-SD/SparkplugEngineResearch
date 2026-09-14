// Own bounded CLI around the one shared recovered source preparation.
#include "../../Sparkplug/Code/SparkplugPC/spPCShaderSource.h"
#include <fstream>
#include <iostream>
#include <stdexcept>
#include <vector>
#include <io.h>
#include <fcntl.h>
static std::string Read(const char* path) {
  std::ifstream input(path,std::ios::binary|std::ios::ate);
  if(!input)throw std::runtime_error("source input unavailable");
  const auto size=input.tellg();if(size<0||size>65536)throw std::runtime_error("source input exceeds bound");
  std::string result(static_cast<size_t>(size),'\0');input.seekg(0);
  if(size&&!input.read(result.data(),size))throw std::runtime_error("incomplete source input");return result;
}
static uint32_t Word(const char* value) {
  size_t end=0;const auto number=std::stoull(value,&end,10);
  if(value[end]||number>UINT32_MAX)throw std::runtime_error("invalid shader key");return uint32_t(number);
}
int main(int argc,char** argv) {
  try {
    if(argc!=5)throw std::runtime_error("usage: shader_source MASK LIGHTS CODE_FILE DECLARATION_FILE");
    const std::array<uint32_t,2> key={Word(argv[1]),Word(argv[2])};
    const auto input=sparkplug::reconstruction::BuildPCShaderSourceInputsForAnalysis(key);
    const auto source=sparkplug::reconstruction::ComposePCShaderSourceForAnalysis(Read(argv[3]),Read(argv[4]),input.insertion,input.header);
    if(_setmode(_fileno(stdout),_O_BINARY)==-1)throw std::runtime_error("binary stdout unavailable");
    std::cout.write(source.data(),source.size());if(!std::cout)throw std::runtime_error("source output failed");return 0;
  }catch(const std::exception& error){std::cerr<<error.what()<<'\n';return 1;}
}
