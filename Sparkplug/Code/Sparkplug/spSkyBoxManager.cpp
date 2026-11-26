#include "spSkyBoxManager.h"
#include "spSkyBox.h"

namespace sparkplug::reconstruction
{
    bool spSkyBoxManager::AppendSkyBoxForAnalysis(CapturedSkyBoxForAnalysis skyBox)
    {
        if (skyBox.address == 0 || skyBox.classId != spSkyBox::ClassID)
            return false;

        // PC 479EE0 inserts a fresh owned list node with a borrowed payload.
        skyBoxes_.push_back(skyBox.address);
        return true;
    }
}
