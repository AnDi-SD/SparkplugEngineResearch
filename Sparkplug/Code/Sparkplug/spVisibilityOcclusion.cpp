// Original46C6B8/46B9BC occluder branch, separated from partition traversal.
#include "spVisibilityManager.h"
#include "spOcclusionVolume.h"
#include "Analysis/PC/spVisibilityPointMath.h"

namespace sparkplug::reconstruction
{
    spVisibilityManager::OccluderSubmissionForAnalysis
    spVisibilityManager::TrySubmitOccluderForAnalysis(spOcclusionVolume& volume,
        const PlaneSetForAnalysis& planes,bool unclipped)
    {
        if(volume.GetVisibilityMarkForAnalysis()==frameStamp_)return {true,false};
        if(unclipped)
        {
            //46B9F6 stamps before append, without a geometry/Enabled test.
            volume.SetVisibilityMarkForAnalysis(frameStamp_);
            visibleOccluders_.push_back(&volume);return {true,true};
        }
        using Class=evidence::pc::visibility_math::SphereClass;
        const auto classification=evidence::pc::visibility_math::Classify(planes,
            volume.GetWorldSphereForAnalysis());
        bool accepted=classification==Class::Inside;
        if(classification==Class::Intersecting)
        {
            // Native dereferences worldVB only on this branch. Unrepresented
            // buffer state is incomplete, not a successful empty point query.
            if(!volume.IsInitializedForAnalysis()||!volume.GetLocalVertexBufferForAnalysis()||
                volume.GetPointsForAnalysis().size()>4096)return {};
            const auto outside=evidence::pc::visibility_point_math::IsOutsidePoints(planes,
                volume.GetPointsForAnalysis());
            if(!outside.completed)return {};
            //46C749 testsAL and branches past append when AL!=0.
            accepted=!outside.value;
        }
        if(!accepted)return {true,false};
        visibleOccluders_.push_back(&volume);
        volume.SetVisibilityMarkForAnalysis(frameStamp_); // normal branch stamps after append
        return {true,true};
    }
}
