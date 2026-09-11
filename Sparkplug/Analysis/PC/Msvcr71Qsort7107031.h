#pragma once

// Versioned CRT dependency, not a recovered Sparkplug class. Exact CFG of
// MSVCR71 7.10.7031.4: qsort7C382650, shortsort7C3825E0. DLL SHA256:
// DCA0E5FAF6C94B6ADFF4D90D40795D5A91BA3A3059EA408E992A0F039A494D46.
// The historical game-distribution DLL version is not established.
#include <cstddef>

namespace sparkplug::host::msvcr71_7_10_7031_4
{
using Compare = int (*)(const void* context,const void* left,const void* right);
// Preserves comparator call order, equal-key permutations and whole records.
// No stable tie-break and no strict-weak-order requirement are introduced.
// Borrowed input/callbacks must remain valid synchronously; callback must not
// change the records. The host rejects null pointers and >INT32_MAX bytes
// before mutation. Original count<2 or width0 return without comparisons.
[[nodiscard]] bool Sort(void* records,std::size_t count,std::size_t width,
    Compare compare,const void* context);
}
