#include "../src/Adapter/PassivePoseSmoother.h"
#include <cmath>
#include <cstring>
#include <iostream>
#include <limits>
#include <stdexcept>

using winxcoop::adapter::PassivePoseSmoother;
using winxcoop::adapter::Transform;
namespace
{
    unsigned checks = 0;
    void Check(bool ok, const char* text)
    { ++checks; if (!ok) throw std::runtime_error(text); }
    bool Near(double a, double b, double tolerance = 0.00003) { return std::abs(a-b) <= tolerance; }
    Transform Pose(float x = 0)
    {
        Transform pose{}; pose.position[0] = x;
        pose.scale[0] = pose.scale[1] = pose.scale[2] = 1;
        pose.orientation[0] = pose.orientation[4] = pose.orientation[8] = 1;
        return pose;
    }
    Transform Heading(double degrees)
    {
        auto pose = Pose(); const auto angle = degrees * 3.14159265358979323846 / 180;
        const float c = static_cast<float>(std::cos(angle)), s = static_cast<float>(std::sin(angle));
        pose.orientation[0] = pose.orientation[8] = c;
        pose.orientation[2] = s; pose.orientation[6] = -s;
        return pose;
    }
    void Proper(const Transform& pose)
    {
        for (const float value : pose.position) Check(std::isfinite(value), "finite display position");
        for (unsigned a = 0; a < 3; ++a) for (unsigned b = 0; b < 3; ++b)
        {
            double dot = 0;
            for (unsigned i = 0; i < 3; ++i) dot += double(pose.orientation[a*3+i]) * pose.orientation[b*3+i];
            Check(Near(dot, a == b ? 1 : 0), "display orientation stays orthonormal");
        }
        const auto* m = pose.orientation;
        const double determinant = double(m[0])*(double(m[4])*m[8]-double(m[5])*m[7]) -
            double(m[1])*(double(m[3])*m[8]-double(m[5])*m[6]) + double(m[2])*(double(m[3])*m[7]-double(m[4])*m[6]);
        Check(Near(determinant, 1), "display orientation keeps determinant+1");
    }
    void DuplicateStopFinishes()
    {
        PassivePoseSmoother smooth; Transform out{};
        Check(smooth.Reset(Pose(), 0) && smooth.SetTarget(Pose(100), 0), "initial motion target");
        Check(smooth.Sample(20, out) && Near(out.position[0], 40), "initial segment advances");
        Check(smooth.SetTarget(Pose(100), 20), "first identical stop sample accepted");
        Check(smooth.SetTarget(Pose(100), 40), "second identical stop sample accepted");
        Check(smooth.Sample(50, out) && out.position[0] == 100, "duplicates preserve first endpoint deadline");
        for (std::uint64_t time : {60ULL, 110ULL, 5000ULL})
        {
            Check(smooth.SetTarget(Pose(100), time) && smooth.Sample(time, out), "stationary updates remain accepted");
            Check(out.position[0] == 100, "stationary stream does not slide or extrapolate");
        }
    }
    void IrregularArrivalAndRotation()
    {
        PassivePoseSmoother smooth; Transform out{};
        Check(smooth.Reset(Pose(), 0) && smooth.SetTarget(Pose(20), 0), "irregular stream begins");
        float destination = 20;
        for (std::uint64_t time : {40ULL, 95ULL, 120ULL, 205ULL, 235ULL, 330ULL})
        {
            Check(smooth.Sample(time, out), "sample before retarget");
            const auto before = out; destination += 20;
            Check(smooth.SetTarget(Pose(destination), time) && smooth.Sample(time, out), "ordinary retarget accepted");
            Check(std::memcmp(&before, &out, sizeof(out)) == 0, "ordinary retarget is continuous");
            Check(out.position[0] >= 0 && out.position[0] <= destination, "position remains within observed span");
            Proper(out);
        }
        Check(smooth.Sample(430, out) && out.position[0] == destination, "bounded horizon finishes after stream stops");
        smooth.Clear();
        Check(smooth.Reset(Heading(170), 0) && smooth.SetTarget(Heading(-170), 0), "stationary turn crosses180");
        Check(smooth.Sample(25, out) && Near(out.orientation[0], -1), "turn chooses shortest arc rather than long spin");
        Proper(out);
        Check(smooth.Sample(50, out) && Near(out.orientation[2], Heading(-170).orientation[2]), "stationary turn reaches target");
        Check(smooth.SetTarget(Heading(80), 50) && smooth.Sample(75, out), "orientation-only target is not treated as duplicate");
        Check(!Near(out.orientation[0], Heading(-170).orientation[0]), "orientation-only target advances");
        Proper(out);
    }
    void GapAndDiscontinuity()
    {
        PassivePoseSmoother smooth; Transform out{};
        Check(smooth.Reset(Pose(), 0) && smooth.SetTarget(Pose(20), 0), "gap stream begins");
        Check(smooth.SetTarget(Pose(40), 500) && smooth.Sample(500, out), "resumed stream accepted");
        Check(out.position[0] == 40, "long gap snaps rather than inventing a path");
        Check(smooth.SetTarget(Pose(552), 520) && smooth.Sample(520, out), "large correction accepted");
        Check(out.position[0] == 552, "large correction is not swept through the scene");
        Check(smooth.SetTarget(Pose(562), 580) && smooth.Sample(580, out), "ordinary movement after correction");
        Check(out.position[0] == 552, "ordinary target resumes interpolation");
        Check(smooth.Sample(680, out) && out.position[0] == 562, "post-correction segment finishes");
    }
    void RejectionAndCohortReset()
    {
        PassivePoseSmoother smooth; Transform out{}, bad = Pose(100);
        Check(!smooth.SetTarget(Pose(), 0), "target requires a qualified cohort");
        Check(smooth.Reset(Pose(), 0) && smooth.SetTarget(Pose(100), 0), "validation path begins");
        bad.position[0] = (std::numeric_limits<float>::quiet_NaN)();
        Check(!smooth.SetTarget(bad, 1000), "nonfinite future target rejected");
        bad = Pose(1000); bad.orientation[0] = -1;
        Check(!smooth.SetTarget(bad, 1000), "invalid discontinuity orientation rejected");
        bad = Pose(1000); bad.scale[0] = 2;
        Check(!smooth.SetTarget(bad, 1000) && !smooth.Reset(bad, 1000), "reset policy cannot change qualified scale");
        Check(smooth.Sample(25, out) && Near(out.position[0], 50), "rejected future targets preserve old path and time");
        Check(!smooth.SetTarget(Pose(100), 24), "duplicate target cannot rewind clock");
        Check(!smooth.Reset(Pose(), 24), "explicit reset cannot rewind active clock");
        const auto before = out;
        Check(!smooth.Sample(24, out) && std::memcmp(&before, &out, sizeof(out)) == 0, "failed sample preserves output");
        Check(smooth.SetTarget(Pose(110), 30) && smooth.Sample(30, out), "rejections leave normal target admission usable");
        Check(Near(out.position[0], 60), "invalid targets did not disturb original segment");
        smooth.Clear();
        Check(!smooth.Active() && !smooth.Sample(0, out), "clear invalidates old cohort");
        auto next = Pose(700); next.scale[0] = 2;
        Check(smooth.Reset(next, 0), "new cohort can use fresh scale and clock");
        next.position[0] = 710;
        Check(smooth.SetTarget(next, 0) && smooth.Sample(25, out), "fresh cohort starts normal segment without stale snap history");
        Check(Near(out.position[0], 705) && out.scale[0] == 2, "fresh cohort has only its own pose history");
    }
}

int main()
{
    try
    {
        DuplicateStopFinishes(); IrregularArrivalAndRotation(); GapAndDiscontinuity(); RejectionAndCohortReset();
        std::cout << "PassivePoseSmoother checks passed: " << checks << '\n';
        return 0;
    }
    catch (const std::exception& error)
    {
        std::cerr << "PassivePoseSmoother failure after " << checks << " checks: " << error.what() << '\n';
        return 1;
    }
}
