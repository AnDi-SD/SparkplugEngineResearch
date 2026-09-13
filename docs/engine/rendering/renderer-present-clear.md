# PC Clear / Present boundary

## Clear4BB980

Secondary `this=complete+18`; arguments flags, ARGB, stencil. Reads the low
flag byte and maps only bits0..2. Calls device complete+C9E8, COM slot+AC,
with rectangle count0, NULL rectangles, `flags&7`, unchanged ARGB, float
depth1.0, unchanged stencil. Returns AL1 even for a failing HRESULT.
All256 low-byte combinations plus100/107/80000000/FFFFFFFF are tested both
with success and failure HRESULTs:260 inputs per native child.

## Present4BB9F0

Complete+C1C0 nonzero returns AL0 without touching the device. Otherwise
COM slot44 receives four NULL arguments. Any HRESULT except88760868
(DEVICELOST), including generic failure, leads to AL1. DEVICELOST calls
COM slot0C (TestCooperativeLevel). Only88760869 (DEVICENOTRESET) follows
the internal reset path; other cooperative results return AL1.

The reset path copies32 bytes complete+1C..3B onto the stack and prepares secondary vtable slot8/+20=4BD810 with a pointer to that copy. This is not a successful native Present/reset execution. The reset helper and device/resource rebuilding remain unknown. The wrapper's one stack argument is not read in this body.

Portable `PresentBeforeResetForAnalysis` returns an explicit incomplete/
reset-required boundary instead of inventing reset behavior. A NULL external
callback is a host guard, not a native behavior claim. Clear and Present APIs
use analysis names and do not imply a complete native renderer layout.
