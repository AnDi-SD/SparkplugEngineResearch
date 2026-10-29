#include "../src/Adapter/RemotePoseInterpolator.h"
#include <cmath>
#include <cstring>
#include <iostream>
#include <limits>
#include <stdexcept>

using winxcoop::adapter::RemotePoseInterpolator;
using winxcoop::adapter::Transform;
namespace
{
    unsigned checks = 0;
    constexpr double Pi = 3.14159265358979323846;
    void Check(bool condition, const char* text)
    { ++checks; if (!condition) throw std::runtime_error(text); }
    bool Near(double a, double b, double tolerance = 0.00002) { return std::abs(a-b) <= tolerance; }
    Transform Pose()
    {
        Transform p{};
        p.scale[0] = p.scale[1] = p.scale[2] = 1;
        p.orientation[0] = p.orientation[4] = p.orientation[8] = 1;
        return p;
    }
    Transform Rotation(unsigned axis, double degrees)
    {
        Transform p = Pose();
        const float c = static_cast<float>(std::cos(degrees*Pi/180)), s = static_cast<float>(std::sin(degrees*Pi/180));
        if (axis == 0) { p.orientation[4]=c; p.orientation[5]=-s; p.orientation[7]=s; p.orientation[8]=c; }
        if (axis == 1) { p.orientation[0]=c; p.orientation[2]=s; p.orientation[6]=-s; p.orientation[8]=c; }
        if (axis == 2) { p.orientation[0]=c; p.orientation[1]=-s; p.orientation[3]=s; p.orientation[4]=c; }
        return p;
    }
    void MatrixNear(const Transform& actual, const Transform& expected)
    { for (unsigned i=0;i<9;++i) Check(Near(actual.orientation[i],expected.orientation[i]), "expected analytic orientation"); }
    void Proper(const Transform& p)
    {
        const auto* m=p.orientation;
        for (unsigned a=0;a<3;++a) for (unsigned b=0;b<3;++b)
        {
            double dot=0;
            for (unsigned i=0;i<3;++i) dot+=static_cast<double>(m[a*3+i])*m[b*3+i];
            Check(Near(dot,a==b?1:0,0.000002),"orthonormal output rows");
        }
        const double det=static_cast<double>(m[0])*(m[4]*m[8]-m[5]*m[7])
            - static_cast<double>(m[1])*(m[3]*m[8]-m[5]*m[6])+static_cast<double>(m[2])*(m[3]*m[7]-m[4]*m[6]);
        Check(Near(det,1,0.000002),"proper output rotation determinant+1");
    }
    void LifecycleAndLinearPosition()
    {
        RemotePoseInterpolator smooth;
        Transform output=Pose(), sentinel=output; output.position[0]=-900; sentinel=output;
        Check(!smooth.Active(),"initially inactive");
        Check(!smooth.Sample(0,output) && std::memcmp(&output,&sentinel,sizeof(output))==0,"inactive sample preserves output");
        Check(!smooth.SetTarget(Pose(),0),"target requires explicit qualified reset");
        Check(smooth.Reset(Pose(),100) && smooth.Active(),"activate qualified pose");
        Transform target=Pose(); target.position[0]=75; target.position[1]=150; target.position[2]=-75;
        Check(smooth.SetTarget(target,100),"default75ms target");
        Check(smooth.Sample(100,output) && output.position[0]==0,"no initial target jump");
        Check(smooth.Sample(125,output),"one-third sample");
        Check(Near(output.position[0],25) && Near(output.position[1],50) && Near(output.position[2],-25),"linear three-axis position");
        Check(smooth.Sample(175,output) && output.position[0]==75,"endpoint reached");
        Check(smooth.Sample(10000,output) && output.position[0]==75,"late sample holds endpoint without extrapolation");
        Proper(output);
        smooth.Clear(); Check(!smooth.Active(),"clear removes active cohort");
        Check(smooth.Reset(Pose(),0),"new cohort may establish a fresh local clock");
    }
    void RotationsAndShortestArc()
    {
        for (unsigned axis=0;axis<3;++axis)
        {
            RemotePoseInterpolator smooth; Transform out;
            Check(smooth.Reset(Pose(),0) && smooth.SetTarget(Rotation(axis,90),0,100),"quarter turn target");
            Check(smooth.Sample(25,out),"quarter interval"); MatrixNear(out,Rotation(axis,22.5)); Proper(out);
            Check(smooth.Sample(50,out),"half interval"); MatrixNear(out,Rotation(axis,45)); Proper(out);
            Check(smooth.Sample(100,out),"quarter-turn endpoint"); MatrixNear(out,Rotation(axis,90));

            smooth.Clear(); Transform halfTurn=Pose();
            for (unsigned i=0;i<3;++i) halfTurn.orientation[i*3+i]=i==axis?1:-1;
            Check(smooth.Reset(Pose(),0) && smooth.SetTarget(halfTurn,0,100),"exact180-degree target");
            Check(smooth.Sample(50,out),"exact180 midpoint"); Proper(out);
            // Exactly180 has two equally short directions. Either90-degree
            // midpoint is valid; a reflected or collapsed matrix is not.
            Check(Near(out.orientation[0]+out.orientation[4]+out.orientation[8],1),"180 midpoint has90-degree trace");
            Check(Near(out.orientation[axis*3+axis],1),"rotation axis preserved");
            Check(smooth.Sample(100,out),"180 endpoint"); MatrixNear(out,halfTurn); Proper(out);
        }
        RemotePoseInterpolator wrap; Transform out;
        Check(wrap.Reset(Rotation(1,170),0) && wrap.SetTarget(Rotation(1,-170),0,100),"wrap across180");
        Check(wrap.Sample(25,out),"shortest-arc quarter"); MatrixNear(out,Rotation(1,175));
        Check(wrap.Sample(50,out),"shortest-arc midpoint"); MatrixNear(out,Rotation(1,180)); Proper(out);
        Check(wrap.Sample(75,out),"shortest-arc three-quarter"); MatrixNear(out,Rotation(1,-175));
        wrap.Clear();
        Check(wrap.Reset(Pose(),0) && wrap.SetTarget(Rotation(2,0.001),0,100),"nearly coincident rotations");
        Check(wrap.Sample(50,out),"near-coincident midpoint"); MatrixNear(out,Rotation(2,0.0005)); Proper(out);
    }
    void ReplacementIsContinuous()
    {
        RemotePoseInterpolator smooth; Transform first=Rotation(1,90), replacement=Rotation(1,180), out;
        first.position[0]=100; replacement.position[0]=200;
        Check(smooth.Reset(Pose(),0) && smooth.SetTarget(first,0,100),"first moving target");
        Check(smooth.Sample(25,out) && Near(out.position[0],25),"first segment sampled");
        // No Sample(50) call precedes replacement: SetTarget itself must find
        // the current old-segment pose, not the old start or endpoint.
        Check(smooth.SetTarget(replacement,50,100),"replace in flight");
        Check(smooth.Sample(50,out) && Near(out.position[0],50),"replacement preserves current position");
        MatrixNear(out,Rotation(1,45));
        Check(smooth.Sample(100,out) && Near(out.position[0],125),"new segment begins at current interpolated position");
        MatrixNear(out,Rotation(1,112.5)); Proper(out);
        Check(smooth.Sample(150,out) && Near(out.position[0],200),"replacement reaches new target");
        MatrixNear(out,Rotation(1,180));
    }
    void RejectBadPoseAndScale()
    {
        const float nan=(std::numeric_limits<float>::quiet_NaN)(), infinity=(std::numeric_limits<float>::infinity)();
        for (unsigned i=0;i<3;++i)
        {
            RemotePoseInterpolator smooth; Transform bad=Pose(); bad.position[i]=nan;
            Check(!smooth.Reset(bad,0) && !smooth.Active(),"nonfinite position cannot activate");
            bad=Pose(); bad.scale[i]=infinity;
            Check(!smooth.Reset(bad,0),"nonfinite scale rejected");
            bad=Pose(); bad.scale[i]=0; Check(!smooth.Reset(bad,0),"zero scale rejected");
            bad.scale[i]=-1; Check(!smooth.Reset(bad,0),"negative scale rejected");
        }
        for (unsigned i=0;i<9;++i)
        {
            RemotePoseInterpolator smooth; Transform bad=Pose(); bad.orientation[i]=infinity;
            Check(!smooth.Reset(bad,0),"nonfinite matrix rejected");
        }
        RemotePoseInterpolator smooth; Transform bad=Pose(), out;
        bad.orientation[0]=-1; Check(!smooth.Reset(bad,0),"reflection rejected despite unit rows");
        bad=Pose(); bad.orientation[1]=0.2f; Check(!smooth.Reset(bad,0),"skew rejected");
        bad=Pose(); bad.orientation[0]=2; Check(!smooth.Reset(bad,0),"matrix scale rejected");
        bad=Pose(); bad.orientation[0]+=0.001f;
        Check(smooth.Reset(bad,0) && smooth.Sample(0,out),"bounded native roundoff accepted"); Proper(out);
        smooth.Clear(); Transform fixed=Pose(); fixed.scale[0]=1; fixed.scale[1]=2; fixed.scale[2]=3;
        Check(smooth.Reset(fixed,0),"qualified nonuniform scale");
        Transform target=fixed; target.position[0]=100;
        Check(smooth.SetTarget(target,0,100),"fixed-scale moving target");
        Transform changed=target; changed.scale[1]=4;
        Check(!smooth.SetTarget(changed,20) && !smooth.Reset(changed,20),"active scale cannot change through target or reset");
        bad=target; bad.position[0]=nan; Check(!smooth.SetTarget(bad,80),"bad future target rejected");
        Check(smooth.Sample(50,out) && Near(out.position[0],50),"rejected target neither advances clock nor changes path");
        for (unsigned i=0;i<3;++i) Check(out.scale[i]==fixed.scale[i],"qualified scale preserved exactly");
        smooth.Clear(); Check(smooth.Reset(changed,0),"clear permits a new qualified scale");
    }
    void TimeAndFiniteExtremes()
    {
        RemotePoseInterpolator smooth; Transform target=Pose(), out=Pose(); target.position[0]=100;
        Check(smooth.Reset(Pose(),100),"monotonic initial clock");
        Check(!smooth.SetTarget(target,99) && !smooth.Reset(Pose(),99),"target and reset cannot rewind active time");
        Check(!smooth.SetTarget(target,100,0) && !smooth.SetTarget(target,100,1001),"duration bounds");
        Check(smooth.SetTarget(target,100,1000) && smooth.Sample(600,out) && Near(out.position[0],50),"maximum permitted duration");
        Transform before=out;
        Check(!smooth.Sample(599,out) && std::memcmp(&out,&before,sizeof(out))==0,"clock rewind leaves output unchanged");
        Check(smooth.Sample(600,out) && out.position[0]==before.position[0],"same timestamp is deterministic");
        smooth.Clear();
        const auto maximum=(std::numeric_limits<std::uint64_t>::max)();
        Check(smooth.Reset(Pose(),maximum-100) && smooth.SetTarget(target,maximum-100,75),"bounded near-limit clock");
        Check(!smooth.SetTarget(target,maximum-50,75),"segment end cannot overflow local time");
        Check(smooth.Sample(maximum-75,out) && Near(out.position[0],100.0/3),"overflow rejection leaves earlier accepted segment intact");
        Check(smooth.Sample(maximum,out) && out.position[0]==100,"near-limit endpoint clamp");
        smooth.Clear(); Transform first=Pose(), last=Pose();
        const float largest=(std::numeric_limits<float>::max)(); first.position[0]=-largest; last.position[0]=largest;
        Check(smooth.Reset(first,0) && smooth.SetTarget(last,0,100),"finite extreme positions accepted");
        Check(smooth.Sample(50,out) && std::isfinite(out.position[0]) && out.position[0]==0,"convex interpolation does not overflow float difference");
        Check(smooth.Sample(100,out) && out.position[0]==largest,"finite extreme endpoint preserved");
    }
}

int main()
{
    try
    {
        LifecycleAndLinearPosition(); RotationsAndShortestArc(); ReplacementIsContinuous();
        RejectBadPoseAndScale(); TimeAndFiniteExtremes();
        std::cout << "RemotePoseInterpolator checks passed: " << checks << '\n';
        return 0;
    }
    catch (const std::exception& error)
    {
        std::cerr << "RemotePoseInterpolator failure after " << checks << " checks: " << error.what() << '\n';
        return 1;
    }
}
