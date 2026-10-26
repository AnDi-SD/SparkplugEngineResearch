#pragma once
#include "Analysis/Host/wxLogoResourceLoadingHost.h"

namespace winx::reconstruction
{
    // Original PC597480 / PS228F720. Logical standalone operation; its lost
    // original C++ declaration owner/name is unknown. This does not assign a
    // new native member method to LogoAnimGameFlowState or another class.
    void LoadLogoSharedResourcesForAnalysis(wxLogoResourceLoadingHost&);
}
