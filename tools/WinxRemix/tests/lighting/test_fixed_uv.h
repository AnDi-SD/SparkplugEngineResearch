#pragma once
// Own numeric fixtures; the only UV implementation is the shared Fixed helper.
static void UvFixtures() {
    using Matrix=pc::FixedUvRegistersForAnalysis;
    const Matrix identity{{{1,0,0,0},{0,1,0,0},{0,0,1,0}}};
    struct Case {Matrix columns;std::array<float,3> expected;};
    const Case cases[]={
      {identity,{.25f,.75f,1}},
      {{{{1,0,0,7},{0,1,0,8},{.5f,-.25f,1,9}}},{.75f,.5f,1}},
      {{{{1,0,.5f,0},{0,1,-.25f,0},{0,0,1,0}}},{.25f,.75f,.9375f}},
      {{{{2,3,0,0},{-1,4,0,0},{0,0,1,0}}},{-.25f,3.75f,1}},
      {{{{0,-1,0,0},{1,0,0,0},{0,0,1,0}}},{.75f,-.25f,1}},
      {{{{1,0,.5f,0},{0,1,-.25f,0},{2,-1,3,0}}},{2.25f,-.25f,2.9375f}},
      {{{{1,0,0,0},{0,1,0,0},{0,0,0,0}}},{.25f,.75f,0}},
    };
    std::array<float,3> output{};
    for(const auto& value:cases) {
        Check(pc::TransformFixedUvForAnalysis({.25f,.75f},value.columns,output),"finite Fixed UV matrix accepts");
        Check(output==value.expected,"column-major UV output retains translation and homogeneous component without division");
    }
    auto padding=identity;for(auto& column:padding)column[3]=NAN;
    Check(pc::TransformFixedUvForAnalysis({.25f,.75f},padding,output)&&output==cases[0].expected,"untouched register padding is not shader input");
    const auto previous=output;auto invalid=identity;invalid[1][0]=INFINITY;
    Check(!pc::TransformFixedUvForAnalysis({.25f,.75f},invalid,output)&&output==previous,"nonfinite matrix preserves prior output");
    Check(!pc::TransformFixedUvForAnalysis({NAN,.75f},identity,output)&&output==previous,"nonfinite UV preserves prior output");
    invalid=identity;invalid[0][0]=3e38f;
    Check(!pc::TransformFixedUvForAnalysis({2,.75f},invalid,output)&&output==previous,"overflow preserves prior output");
}

static void ReplayUv(const char* source,const char* destination) {
    const auto bytes=Read(source,12+256*60);Check(bytes.size()>=12,"FUV1 header present");
    uint32_t header[3]{};memcpy(header,bytes.data(),12);
    Check(header[0]==0x31565546&&header[1]==1&&header[2]>0&&header[2]<=256&&bytes.size()==12+header[2]*60,"FUV1 bounded record layout");
    std::vector<float> values;values.reserve(header[2]*3);
    for(unsigned i=0;i<header[2];++i) {
        const auto record=bytes.data()+12+i*60;uint32_t enabled=0;memcpy(&enabled,record,4);
        std::array<float,2> input{};memcpy(input.data(),record+4,8);
        pc::FixedUvRegistersForAnalysis matrix{};memcpy(matrix.data(),record+12,48);
        Check(enabled<=1&&std::isfinite(input[0])&&std::isfinite(input[1]),"FUV1 UV mode and inputs");
        std::array<float,3> output{input[0],input[1],1};
        if(enabled)Check(pc::TransformFixedUvForAnalysis(input,matrix,output),"shared Fixed UV evaluation");
        values.insert(values.end(),output.begin(),output.end());
    }
    std::ofstream stream(destination,std::ios::binary);Check(bool(stream),"UV output open");
    stream.write(reinterpret_cast<const char*>(values.data()),values.size()*4);stream.close();Check(bool(stream),"complete UV output and close");
    printf("{\"status\":\"PASS\",\"records\":%u,\"stride\":12,\"gpu\":false}\n",header[2]);
}
