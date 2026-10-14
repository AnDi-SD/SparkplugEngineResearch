#include "Code/SparkplugDX/spDXAsmShaderParser.h"
#include <iostream>
#include <stdexcept>

using namespace sparkplug::reconstruction;

namespace
{
    unsigned checks=0;
    void Check(bool value,const char* message)
    {
        ++checks;
        if(!value)throw std::runtime_error(message);
    }
}

int main()
{
    try
    {
        const auto& record=spDXAsmShaderParser::StaticRTTI();
        Check(record.classID==0x0B3E0AA9,"original class identity");
        Check(record.baseClassID==spBaseObject::ClassID,"registration BaseObject parent");
        Check(record.factory!=nullptr,"original concrete factory");
        auto source=record.factory();
        Check(dynamic_cast<spDXAsmShaderParser*>(source.get())!=nullptr,"factory produces declared type");
        Check(source->IsExactly(spDXAsmShaderParser::ClassID),"original exact identity getter");
        Check(source->IsKindOf(spBaseObject::ClassID),"original base identity");
        Check(!source->IsExactly(spBaseObject::ClassID),"own identity differs from base");
        Check(spDXAsmShaderParser::NativeObjectSizeForAnalysis==16,"actual original factory extent");
        auto clone=source->Clone();
        Check(clone&&clone.get()!=source.get(),"native clone creates a separate object");
        Check(dynamic_cast<spDXAsmShaderParser*>(clone.get())!=nullptr,"clone uses its concrete factory");
        Check(clone->IsExactly(spDXAsmShaderParser::ClassID),"clone preserves class identity");
        clone.reset();source.reset();
        std::cout<<"PASS "<<checks<<" ASM parser lifecycle checks; assembly grammar/compiler not executed\n";
        return 0;
    }
    catch(const std::exception& error)
    {
        std::cerr<<"FAIL "<<error.what()<<'\n';
        return 1;
    }
}
