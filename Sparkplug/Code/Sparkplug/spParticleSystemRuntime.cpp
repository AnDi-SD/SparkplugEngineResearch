#include "spParticleSystem.h"
#include "spFunctionEval.h"
#include "spRenderNode.h"
#include "Analysis/PC/spAxisAngleMath.h"
#include "Analysis/PC/spParticleCpuUpdate.h"
#include "Analysis/PC/spParticleEmissionBudget.h"
#include <cmath>
#include <cstring>

namespace sparkplug::reconstruction
{
    namespace
    {
        namespace math=evidence::pc::node_math;
        using Vector3=spParticleSystem::Vector3;
        constexpr double Unit=0x1p-32;
        constexpr float Epsilon=0.001f,TwoPiUnit=0x1.921fb6p-30f;
        constexpr float Degree=0x1.1df46ap-6f,HalfPi=0x1.921fb6p+0f,InvSqrt3=0x1.279a72p-1f;
        std::uint32_t Bits(float value) noexcept
        {std::uint32_t result;std::memcpy(&result,&value,4);return result;}
        float Float(std::uint32_t value) noexcept
        {float result;std::memcpy(&result,&value,4);return result;}
        Vector3 Perpendicular(const Vector3& direction)
        {
            const double sum=(double(direction[2])+direction[1])+direction[0];
            const Vector3 helper=std::abs(std::abs(sum*InvSqrt3)-1.0)<=double(Epsilon)
                ?Vector3{1,0,0}:Vector3{1,1,1};
            Vector3 result{float(double(direction[1])*helper[2]-double(direction[2])*helper[1]),
                float(double(direction[2])*helper[0]-double(direction[0])*helper[2]),
                float(double(direction[0])*helper[1]-double(direction[1])*helper[0])};
            evidence::pc::NormalizeParticleDirectionForAnalysis(result);return result;
        }
        Vector3 RotateProducer(const Vector3& value,const math::Matrix3& matrix)
        {
            // PC48C981 uses rows of the axis-angle matrix, unlike420350.
            Vector3 result{};
            for(unsigned r=0;r<3;++r)result[r]=float((double(matrix[r*3+2])*value[2]
                +double(matrix[r*3+1])*value[1])+double(matrix[r*3])*value[0]);
            return result;
        }
    }
    bool spParticleSystem::InitializeForAnalysis(evidence::pc::ParticleRandomForAnalysis* supplied,std::string* error)
    {
        if(error)error->clear();
        const auto fail=[&](const char* why){if(error)*error=why;return false;};
        if(initialized_)return fail("Particle CPU pool is already initialized");
        const auto& p=parameters_;
        std::uint32_t count=0;
        // Native zero/one-slot Reset forms pointers outside its link storage.
        // Capacity is a host bound, not the native unsigned65536 clamp.
        if(!CapacityForAnalysis(count)||count<2||!std::isfinite(p.rate)||p.rate<=0)
            return fail("Particle CPU pool requires finite capacity2..1024 and positive rate");
        for(const auto& v:{p.direction,Vector3{p.velocity[0],p.velocity[1],p.times[1]},Vector3{p.angle[0],p.angle[1],0}})
            for(float f:v)if(!std::isfinite(f))return fail("Nonfinite particle producer parameter");
        if(std::abs(p.angle[0])>360000||std::abs(p.angle[1])>360000)
            return fail("Particle angle exceeds the host bounded trigonometric domain");
        constexpr std::array<unsigned,8> regionSizes{0,3,6,4,8,4,5,6};
        if(p.regionType<1||p.regionType>7||p.region.size()!=regionSizes[p.regionType])
            return fail("Particle producer requires one complete emission region");
        for(float f:p.region)if(!std::isfinite(f)||std::abs(f)>65536)
            return fail("Particle emission region exceeds bounded sampler inputs");
        const auto node=renderNode_.lock();
        if(p.flags[0]&&!node)return fail("Looping particle initialization requires a live RenderNode");
        if(node)
        {
            for(float f:node->GetWorldMatrixForAnalysis())if(!std::isfinite(f))
                return fail("Nonfinite particle RenderNode world input");
        }
        records_.assign(count,{});recordWritten_.assign(count,0);links_.resize(count);
        for(std::uint32_t i=0;i<count;++i)links_[i]={i,(i+count-1)%count,(i+1)%count};
        first_=boundary_=0;poolState_={0,count,0,0,0};
        if(!p.flags[0]){initialized_=true;return true;}
        initialized_=true;
        poolState_[4]=Bits(p.times[1]);
        ProductionForAnalysis production;
        if(!ProduceParticlesImplForAnalysis(count,0,production,supplied,error))
        {initialized_=false;return false;}
        poolState_[4]=0;return true;
    }
    bool spParticleSystem::ProduceParticlesForAnalysis(std::uint32_t requested,float time,
        ProductionForAnalysis& output,evidence::pc::ParticleRandomForAnalysis* supplied,std::string* error)
    {
        if(updating_){if(error)*error="Reentrant particle production is outside the host contract";return false;}
        return ProduceParticlesImplForAnalysis(requested,time,output,supplied,error);
    }
    bool spParticleSystem::ProduceParticlesImplForAnalysis(std::uint32_t requested,float time,
        ProductionForAnalysis& output,evidence::pc::ParticleRandomForAnalysis* supplied,std::string* error)
    {
        if(error)error->clear();
        const auto fail=[&](const char* why){if(error)*error=why;return false;};
        if(!initialized_ || records_.size()<2 || records_.size()>1024 || links_.size()!=records_.size()
            || recordWritten_.size()!=records_.size() || poolState_[0]+std::uint64_t(poolState_[1])!=records_.size())
            return fail("Particle producer requires an initialized consistent CPU pool");
        ProductionForAnalysis result;
        result.limitedRequest=std::min(requested,poolState_[1]);
        // Original early return precedes node, sampler, direction and clock reads.
        if(!result.limitedRequest){output=result;return true;}
        const auto& p=parameters_;
        if(!std::isfinite(time) || !std::isfinite(Float(poolState_[4])) || !std::isfinite(p.rate) || p.rate<=0
            || !std::isfinite(p.times[1]) || !(p.times[1]>0))return fail("Invalid live particle producer clock/rate/lifetime");
        for(const auto& v:{p.direction,Vector3{p.velocity[0],p.velocity[1],0},Vector3{p.angle[0],p.angle[1],0}})
            for(float f:v)if(!std::isfinite(f))return fail("Nonfinite live particle producer parameter");
        if(std::abs(p.angle[0])>360000 || std::abs(p.angle[1])>360000)
            return fail("Particle angle exceeds the bounded trigonometric domain");
        const auto node=renderNode_.lock();
        if(!node)return fail("Particle producer requires a live RenderNode");
        for(float f:node->GetWorldMatrixForAnalysis())if(!std::isfinite(f))
            return fail("Nonfinite particle RenderNode input");
        auto& random=supplied?*supplied:spFunctionEval::SharedRandomForAnalysis();
        const auto& world=node->GetWorldOrientationForAnalysis();
        const auto affine=node->GetWorldMatrixForAnalysis();
        const auto direction=math::Transform(p.direction,world);
        const bool zero=std::abs(p.direction[0])<=Epsilon&&std::abs(p.direction[1])<=Epsilon&&std::abs(p.direction[2])<=Epsilon;
        const auto perpendicular=zero?Vector3{}:Perpendicular(direction);
        const auto count=result.limitedRequest;
        const float baseTime=float(double(time)-Float(poolState_[4]));
        const auto batches=(count>>7)+((count&127)!=0);
        const auto range=[&](float low,float high){return (double(high)-low)*double(random.Next())*Unit+low;};
        std::vector<Vector3> positions,velocities,sphere;
        velocities.reserve(128);positions.reserve(128);sphere.reserve(1);
        const std::vector<float> unitSphere{0,0,0,1};
        for(std::uint32_t batch=0;batch<batches;++batch)
        {
            // Original final batch is the remainder even when remainder==0.
            const std::uint32_t size=batch+1==batches?(count&127):128;
            if(!SampleEmissionRegionForAnalysis(random,size,positions))
                return fail("Particle emission sampler exceeded its bounded rejection loop");
            velocities.clear();
            for(std::uint32_t i=0;i<size;++i)
            {
                Vector3 velocity{};
                if(zero)
                {
                    if(!evidence::pc::SampleParticleRegionForAnalysis(3,unitSphere,random,1,sphere))
                        return fail("Particle direction sampler exceeded its bounded rejection loop");
                    velocity=sphere[0];evidence::pc::NormalizeParticleDirectionForAnalysis(velocity);
                    const double speed=range(p.velocity[0],p.velocity[1]);
                    for(float& v:velocity)v=float(double(v)*speed);
                }
                else
                {
                    const float azimuth=float(double(random.Next())*TwoPiUnit);
                    const float low=float(double(Degree)*p.angle[0]),high=float(double(Degree)*p.angle[1]);
                    const double angleValue=range(low,high);const float angle=float(angleValue);
                    if(std::abs(angleValue-HalfPi)<=double(Epsilon))velocity=perpendicular;
                    else
                    {
                        const double cosine=std::cos(double(angle)),sine=std::sin(double(angle));
                        const Vector3 axial{float(double(direction[0])*cosine),float(double(direction[1])*cosine),float(double(direction[2])*cosine)};
                        velocity={float(double(float(double(perpendicular[0])*sine))+axial[0]),
                            float(double(float(double(perpendicular[1])*sine))+axial[1]),
                            float(double(perpendicular[2])*sine+axial[2])};
                    }
                    const double speed=range(p.velocity[0],p.velocity[1]);
                    for(float& v:velocity)v=float(double(v)*speed);
                    velocity=RotateProducer(velocity,math::AxisAngleForAnalysis(azimuth,direction));
                }
                velocities.push_back(velocity);
            }
            for(std::uint32_t i=0;i<size;++i)
            {
                const auto selected=links_[first_][1];auto& record=records_[links_[selected][0]];
                record[7]=p.times[1];record[6]=float(double(batch*128+i)/p.rate+baseTime);
                auto position=positions[i],velocity=velocities[i];
                if(p.flags[1])
                {
                    // PC48CB14: destination-specific addition orders/spills.
                    position={float((double(affine[8])*position[2]+double(affine[4])*position[1])+double(affine[0])*position[0]+affine[12]),
                        float((double(affine[1])*position[0]+double(affine[5])*position[1])+double(affine[9])*position[2]+affine[13]),
                        float((double(affine[2])*position[0]+double(affine[6])*position[1])+double(affine[10])*position[2]+affine[14])};
                    velocity={float((double(world[3])*velocity[1]+double(world[6])*velocity[2])+double(world[0])*velocity[0]),
                        float((double(world[4])*velocity[1]+double(world[1])*velocity[0])+double(world[7])*velocity[2]),
                        float((double(world[5])*velocity[1]+double(world[2])*velocity[0])+double(world[8])*velocity[2])};
                }
                for(unsigned c=0;c<3;++c){record[c]=position[c];record[c+3]=velocity[c];}
                recordWritten_[links_[selected][0]]=1;
                auto insert=first_;
                if(insert!=boundary_)
                {
                    const float end=float(double(time)+p.times[1]);
                    while(insert!=boundary_)
                    {
                        const auto& current=records_[links_[insert][0]];
                        if(double(current[7])+current[6]<=end)break;
                        insert=links_[insert][2];
                    }
                }
                auto& link=links_[selected];
                links_[link[2]][1]=link[1];links_[link[1]][2]=link[2];
                link[2]=insert;link[1]=links_[insert][1];links_[link[1]][2]=selected;links_[insert][1]=selected;
                if(links_[first_][1]==selected)first_=selected;
            }
            poolState_[1]-=size;poolState_[0]+=size;result.created+=size;
        }
        output=result;return true;
    }

    void spParticleSystem::SetFrameStateForAnalysis(FrameStateForAnalysis value) noexcept
    {poolState_[2]=Bits(value.delta);poolState_[3]=Bits(value.clock);poolState_[4]=Bits(value.accumulator);}
    spParticleSystem::FrameStateForAnalysis spParticleSystem::GetFrameStateForAnalysis() const noexcept
    {return {Float(poolState_[2]),Float(poolState_[3]),Float(poolState_[4])};}
    bool spParticleSystem::SetRecordForAnalysis(std::size_t physical,const ParticleRecordForAnalysis& value) noexcept
    {
        if(!initialized_ || physical>=records_.size())return false;
        for(float f:value)if(!std::isfinite(f))return false;
        if(!(value[7]>0))return false;
        records_[physical]=value;recordWritten_[physical]=1;return true;
    }
    bool spParticleSystem::UpdateForAnalysis(float delta,const FrameOptionsForAnalysis& options,
        evidence::pc::ParticleRandomForAnalysis* random,std::string* error)
    {
        if(error)error->clear();
        const auto fail=[&](const char* why){if(error)*error=why;return false;};
        const auto& p=parameters_;const auto old=GetFrameStateForAnalysis();
        if(!initialized_ || updating_)return fail("Particle update requires a non-reentrant initialized pool");
        for(float f:{delta,old.clock,old.accumulator,p.times[1],options.integrationRate})
            if(!std::isfinite(f))return fail("Nonfinite particle frame input");
        if(!(p.times[1]>0) || options.integrationRate<0)return fail("Unsupported particle lifetime/integration rate");
        for(const auto& a:p.acceleration)for(float f:a)if(!std::isfinite(f))return fail("Nonfinite particle acceleration");
        for(const auto& modifier:options.modifiers)if(!modifier.apply)return fail("Particle modifier needs its virtual callback");
        const float effectiveDelta=delta>p.times[1]?p.times[1]:delta;
        const float clock=float(double(old.clock)+delta);
        if(!std::isfinite(clock))return fail("Particle clock overflow");
        std::uint32_t steps=1;
        if(!p.flags[0] && p.flags[2] && options.integrationRate!=0)
        {
            const double n=std::trunc(double(options.integrationRate)*effectiveDelta);
            if(n<0 || n>128)return fail("Particle maintenance exceeds bounded step count");
            steps=std::max(1u,std::uint32_t(n));
        }
        const float frameStart=float(double(clock)-effectiveDelta),subDelta=float(double(effectiveDelta)/steps);
        if(!std::isfinite(frameStart))return fail("Particle frame start overflow");
        struct Updating final {bool& flag;explicit Updating(bool& f):flag(f){flag=true;}~Updating(){flag=false;}} guard(updating_);
        poolState_[2]=Bits(effectiveDelta);poolState_[3]=Bits(clock);
        if(p.flags[0])
        {
            // Original wrap subtracts once, even if delta spans many lifetimes.
            if(clock>p.times[1])poolState_[3]=Bits(float(double(clock)-p.times[1]));
            return true;
        }
        for(std::uint32_t step=1;step<=steps;++step)
        {
            const float time=p.flags[2]?float(double(step)*subDelta+frameStart):clock;
            if(!std::isfinite(time))return fail("Particle maintenance clock overflow");
            // Retire the physical ring suffix. Equality survives this strict test.
            while(poolState_[0])
            {
                const auto last=links_[boundary_][1];const auto physical=links_[last][0];
                const auto& record=records_[physical];
                if(!recordWritten_[physical])return fail("Unwritten active particle record");
                if(!(double(record[6])+record[7]<double(time)))break;
                boundary_=last;--poolState_[0];++poolState_[1];
            }
            evidence::pc::ParticleEmissionBudgetForAnalysis budget;
            if(!evidence::pc::EvaluateParticleEmissionBudgetForAnalysis(
                {Float(poolState_[4]),subDelta,time,p.times[1],p.times[0],p.rate,options.emissionEnabled},budget))
                return fail("Unsupported particle emission budget");
            if(budget.callProducer)
            {
                poolState_[4]=Bits(budget.accumulatorForProducer);
                ProductionForAnalysis production;
                if(!ProduceParticlesImplForAnalysis(budget.requestedCount,time,production,random,error))return false;
                // Debit the original request, ignoring clamping and created count.
                poolState_[4]=Bits(budget.accumulatorAfter);
            }
            // Native virtual+24 takes only delta. Derived modifier algorithms
            // remain explicit borrowed callbacks; they may change record values.
            for(const auto& modifier:options.modifiers)modifier.apply(modifier.context,subDelta);
            if(!p.flags[2])continue;
            auto link=first_;
            for(std::uint32_t n=0;n<poolState_[0];++n,link=links_[link][2])
            {
                const auto physical=links_[link][0];
                if(!recordWritten_[physical] || !evidence::pc::IntegrateParticleCpuRecordForAnalysis(
                    records_[physical],frameStart,subDelta,p.acceleration[0],p.acceleration[1]))
                    return fail("Unsupported particle record integration");
            }
        }
        return true;
    }
    bool spParticleSystem::DrawCpuForAnalysis(const CpuDrawInputForAnalysis& input,
        const CpuDrawDispatchForAnalysis& dispatch,CpuDrawStateForAnalysis& state,std::string* error) const
    {
        if(error)error->clear();
        const auto fail=[&](const char* why){if(error)*error=why;return false;};
        if(!initialized_ || updating_ || !dispatch.setWorld || !dispatch.uploadAndDraw)
            return fail("CPU particle draw needs an idle pool and synchronous dispatch");
        std::array<float,16> world;
        if(!evidence::pc::BuildBallisticCpuWorldMatrixForAnalysis(input.nodePosition,parameters_.flags[1]!=0,world))
            return fail("Invalid particle draw world input");
        //4BB100 tests SetWorld, then resets the byte at effect+1C.
        if(!dispatch.setWorld(dispatch.context,world))return fail("Particle SetWorld refused");
        state.effectField1C=0;state.submittedBatches=state.rejectedSubmissionReplies=0;
        evidence::pc::BallisticCpuBasisForAnalysis basis;
        if(!evidence::pc::BuildBallisticCpuBasisForAnalysis(input.cameraOrientation,input.nodeScale,basis))
            return fail("Invalid particle draw basis");
        std::vector<evidence::pc::BallisticCpuBatchForAnalysis> batches;
        if(!evidence::pc::PlanBallisticCpuBatchesForAnalysis(poolState_[0],input.vertexCapacity,input.indexCapacity,batches))
            return fail("Invalid particle draw capacities");
        std::vector<evidence::pc::BallisticCpuParticleRecord> active;
        active.reserve(poolState_[0]);auto link=first_;
        for(std::uint32_t n=0;n<poolState_[0];++n,link=links_[link][2])
        {
            const auto physical=links_[link][0];
            if(!recordWritten_[physical])return fail("Unwritten active particle draw record");
            active.push_back(records_[physical]);
        }
        const evidence::pc::BallisticCpuParametersForAnalysis parameters{
            Float(poolState_[3]),parameters_.acceleration[0],parameters_.acceleration[1],input.runtimeScales};
        for(const auto& batch:batches)
        {
            evidence::pc::BallisticCpuPackedBatchForAnalysis packed;
            if(!evidence::pc::BuildBallisticCpuPackedBatchForAnalysis(active,batch,parameters,parameters_.flags[2]!=0,
                basis,parameters_.colors[0],parameters_.colors[1],dispatch.color,dispatch.context,packed))
                return fail("Particle quad packing/color provider refused");
            const bool accepted=dispatch.uploadAndDraw(dispatch.context,packed.vertices.data(),
                std::uint32_t(packed.vertices.size()),packed.indices.data(),std::uint32_t(packed.indices.size()),
                2,batch.particleCount*2);
            ++state.submittedBatches;
            if(!accepted)++state.rejectedSubmissionReplies; // Original ignores AL from renderer+2C.
        }
        return true;
    }
}
