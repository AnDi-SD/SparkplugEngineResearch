#include "Analysis/PC/spParticleCpuUpdate.h"
#include "Analysis/PC/spParticleEmissionBudget.h"
#include <iomanip>
#include <iostream>
#include <limits>
#include <stdexcept>
#include <string_view>
using namespace sparkplug::evidence::pc;
namespace
{
    unsigned checks=0;
    void Check(bool condition,const char* label){++checks;if(!condition)throw std::runtime_error(label);}
    int ProbeEmission()
    {
        unsigned count=0;if(!(std::cin>>count) || count>4096)return 2;
        std::cout<<std::setprecision(17)<<'[';
        for(unsigned i=0;i<count;++i)
        {
            ParticleEmissionBudgetInputForAnalysis p;
            std::cin>>p.accumulator>>p.delta>>p.time>>p.lifetimeParameter>>p.duration>>p.rate>>p.enabled;
            if(!std::cin)return 2;
            ParticleEmissionBudgetForAnalysis result;
            const bool accepted=EvaluateParticleEmissionBudgetForAnalysis(p,result);
            std::cout<<(i?",":"")<<"{\"accepted\":"<<(accepted?"true":"false")
                <<",\"callProducer\":"<<(result.callProducer?"true":"false")<<",\"requestedCount\":"<<result.requestedCount
                <<",\"accumulatorForProducer\":"<<result.accumulatorForProducer<<",\"accumulatorAfter\":"<<result.accumulatorAfter<<'}';
        }
        std::cout<<"]\n";return 0;
    }
    int Probe()
    {
        unsigned count=0;if (!(std::cin>>count) || count>256) return 2;
        std::cout<<std::setprecision(17)<<'[';
        for (unsigned i=0;i<count;++i)
        {
            ParticleCpuUpdateParametersForAnalysis p;unsigned recordCount=0;
            std::cin>>p.currentTime>>p.delta>>p.lifetimeParameter>>p.integrationRate>>p.loop>>p.storedPosition;
            for(auto& value:p.accelerationBegin)std::cin>>value;
            for(auto& value:p.accelerationEnd)std::cin>>value;
            std::cin>>recordCount;if(!std::cin || recordCount>1024)return 2;
            std::vector<BallisticCpuParticleRecord> records(recordCount);
            for(auto& record:records)for(auto& value:record)std::cin>>value;
            if(!std::cin)return 2;
            ParticleCpuUpdateForAnalysis result;
            const bool accepted=AdvanceExistingParticleCpuRecordsForAnalysis(records,p,result);
            std::cout<<(i?",":"")<<"{\"accepted\":"<<(accepted?"true":"false");
            if(accepted)
            {
                std::cout<<",\"currentTime\":"<<result.currentTime<<",\"effectiveDelta\":"<<result.effectiveDelta
                    <<",\"frameStart\":"<<result.frameStart<<",\"activeCount\":"<<result.activeCount<<",\"maintenance\":[";
                bool first=true;for(const auto& step:result.maintenance)
                {std::cout<<(first?"":",")<<'['<<step.time<<','<<step.delta<<','<<step.activeBefore<<','<<step.activeAfter<<']';first=false;}
                std::cout<<"],\"records\":[";first=true;
                for(const auto& record:result.records)
                {std::cout<<(first?"":",")<<'[';for(unsigned j=0;j<8;++j)std::cout<<(j?",":"")<<record[j];std::cout<<']';first=false;}
                std::cout<<']';
            }
            std::cout<<'}';
        }
        std::cout<<"]\n";return 0;
    }
}
int main(int argc,char** argv)
{
    if(argc==2 && std::string_view(argv[1])=="--probe")return Probe();
    if(argc==2 && std::string_view(argv[1])=="--probe-emission")return ProbeEmission();
    try
    {
        const std::vector<BallisticCpuParticleRecord> records{{1,2,3,.5f,-.25f,.75f,0,2}};
        ParticleCpuUpdateParametersForAnalysis p{1,.1f,2,30,false,true,{.25f,.5f,1},{.25f,.5f,1}};
        ParticleCpuUpdateForAnalysis result;
        Check(AdvanceExistingParticleCpuRecordsForAnalysis(records,p,result),"finite existing-particle update");
        Check(result.maintenance.size()==3 && result.activeCount==1,"three original substeps");
        Check(std::abs(result.records[0][0]-1.050249934f)<1e-6f,"observed update uses changed velocity in later positions");
        p.delta=0;Check(AdvanceExistingParticleCpuRecordsForAnalysis(records,p,result)
            && result.records[0][0]==1 && result.records[0][3]>.5f,"zero delta still changes velocity once");
        p.loop=true;p.currentTime=5;p.delta=1.5f;
        Check(AdvanceExistingParticleCpuRecordsForAnalysis(records,p,result) && result.currentTime==4.5f
            && result.maintenance.empty() && result.records==records,"loop subtracts period once and skips record update");
        p.loop=false;p.storedPosition=false;p.currentTime=1.9f;p.delta=.1f;
        Check(AdvanceExistingParticleCpuRecordsForAnalysis(records,p,result) && result.activeCount==1,"exact death time remains active");
        p.delta=.2f;
        Check(AdvanceExistingParticleCpuRecordsForAnalysis(records,p,result) && result.activeCount==0
            && result.records==records,"expired records retire without overwriting pool bytes");
        p.currentTime=1;p.storedPosition=true;p.delta=.5f;p.lifetimeParameter=.125f;
        Check(AdvanceExistingParticleCpuRecordsForAnalysis(records,p,result) && result.currentTime==1.5f
            && result.effectiveDelta==.125f && result.frameStart==1.375f,"delta clamp does not clamp clock advance");
        const auto saved=result;
        p.integrationRate=100000;
        Check(!AdvanceExistingParticleCpuRecordsForAnalysis(records,p,result) && result.records==saved.records,"host substep bound preserves output");
        p.integrationRate=30;p.delta=-1;
        Check(!AdvanceExistingParticleCpuRecordsForAnalysis(records,p,result),"negative unsigned-loop conversion refused");
        p.delta=std::numeric_limits<float>::infinity();
        Check(!AdvanceExistingParticleCpuRecordsForAnalysis(records,p,result),"nonfinite delta refused");
        ParticleEmissionBudgetInputForAnalysis emission{.1f,.7f,1.7f,2,-1,5,true};
        ParticleEmissionBudgetForAnalysis budget;
        Check(EvaluateParticleEmissionBudgetForAnalysis(emission,budget) && budget.requestedCount==3
            && budget.accumulatorForProducer==.8f, "request uses wider accumulation before float store");
        emission={.9f,.1f,1.1f,1,-1,10,true};
        Check(EvaluateParticleEmissionBudgetForAnalysis(emission,budget) && budget.requestedCount==9
            && budget.accumulatorForProducer==1 && budget.accumulatorAfter==.1f, "rounded accumulator at cap does not imply ten requests");
        emission={.25f,0,1,2,-1,10,true};
        Check(EvaluateParticleEmissionBudgetForAnalysis(emission,budget) && budget.requestedCount==2
            && budget.accumulatorAfter==.05f, "budget consumes requested count even with no producer capacity input");
        emission={0,.125f,1.125f,2,1.125f,10,true};
        Check(EvaluateParticleEmissionBudgetForAnalysis(emission,budget) && budget.callProducer, "duration equality still emits");
        emission.duration=1;
        Check(EvaluateParticleEmissionBudgetForAnalysis(emission,budget) && !budget.callProducer
            && budget.accumulatorAfter==0, "past duration leaves accumulator unchanged");
        emission.duration=-1;emission.rate=0;
        Check(EvaluateParticleEmissionBudgetForAnalysis(emission,budget) && budget.callProducer
            && budget.requestedCount==0 && budget.accumulatorAfter==.125f, "zero request still calls producer");
        const auto budgetSaved=budget;emission.rate=-1;
        Check(!EvaluateParticleEmissionBudgetForAnalysis(emission,budget)
            && budget.accumulatorAfter==budgetSaved.accumulatorAfter, "unsupported rate refuses without mutation");
        std::cout<<"PASS Particle CPU update: "<<checks<<" checks\n";return 0;
    }
    catch(const std::exception& error){std::cerr<<"FAIL "<<error.what()<<'\n';return 1;}
}
