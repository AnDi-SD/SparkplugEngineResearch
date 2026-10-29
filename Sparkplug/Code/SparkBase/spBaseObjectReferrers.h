#pragma once
#include "Analysis/Host/spBaseReferrerContext.h"

namespace sparkplug::reconstruction
{
    // Responsibility split around PC Base410050 and the list cleanup consumed
    // by Base4102B0. Original C++ names/declarations are unknown. The explicit
    // context is a portable representation of native field04, not its ABI.
    [[nodiscard]] spBaseReferrerResultForAnalysis AppendBaseReferrerForAnalysis(
        spBaseObject& subject,spBaseObject& borrowedReferrer,spBaseReferrerContextForAnalysis&);

    // Terminal cleanup of the represented list metadata only. Existing Base
    // destructors do not discover this external context automatically. Native
    // consumer qualification uses empty-name actual Named subjects; forwarding,
    // removal and other active-list lifetime integrations remain separate.
    [[nodiscard]] spBaseReferrerResultForAnalysis CloseBaseReferrersForAnalysis(
        spBaseObject& subject,spBaseReferrerContextForAnalysis&) noexcept;
}
