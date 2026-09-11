#include "spParticleSystem.h"
#include "spFunctionEval.h"
#include "spRenderNode.h"
#include "Analysis/PC/spAxisAngleMath.h"
#include <cmath>

namespace sparkplug::reconstruction
{
    namespace
    {
        namespace math=evidence::pc::node_math;
        using Vector3=spParticleSystem::Vector3;
        constexpr double Unit=0x1p-32;
        constexpr float Epsilon=0.001f,TwoPiUnit=0x1.921fb6p-30f;
        constexpr float Degree=0x1.1df46ap-6f,HalfPi=0x1.921fb6p+0f,InvSqrt3=0x1.279a72p-1f;
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
        auto& random=supplied?*supplied:spFunctionEval::SharedRandomForAnalysis();
        const auto& world=node->GetWorldOrientationForAnalysis();
        const auto affine=node->GetWorldMatrixForAnalysis();
        const auto direction=math::Transform(p.direction,world);
        const bool zero=std::abs(p.direction[0])<=Epsilon&&std::abs(p.direction[1])<=Epsilon&&std::abs(p.direction[2])<=Epsilon;
        const auto perpendicular=zero?Vector3{}:Perpendicular(direction);
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
                record[7]=p.times[1];record[6]=float(double(batch*128+i)/p.rate+float(-double(p.times[1])));
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
                    const float end=float(0.0+double(p.times[1]));
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
            poolState_[1]-=size;poolState_[0]+=size;
        }
        initialized_=true;return true;
    }
}
