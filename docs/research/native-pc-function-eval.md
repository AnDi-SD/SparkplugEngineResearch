# PC FunctionEval: scalar runtime and shared codec (checkpoint21)

Original PC SHA-256:
`3F022480BF55045DA4BF692E4BC8862ED38FC024E8A964A558FBDFDF646DFC4F`.
PC-only evidence; no PS2 credit. Class/API paths remain inferred except the
actual registered names. This is a dependency of UV, transform and color
controllers, not a new unrelated easy-class selection.

## Identity, lifetime and storage

`spFunctionEval` ID9450E590 → `spEvaluator` E91D088D → BaseObject;
registration760B78/init6D3E10, getter478610, factory478840, vtable6EA9AC,
deleting destructor478660, virtual clone4788D0, Evaluate478680.
Actual factory allocates38 bytes; all tracked allocations are released.

| Offset | Observed role | Factory default |
|---|---|---|
| 10 | runtime accumulated time | 0 |
| 14 / 18 | frequency / reciprocal | 1 / 1 |
| 1C | amplitude | 1 |
| 20 / 24 | X offset / Y offset | 0 / 0 |
| 28 | pitch | 0 |
| 2C / 30 | directional clamp limit / enabled byte | 0 / false |
| 34 | raw function type | 0 |

Padding31..33 stays allocation poison. Native virtual clone calls inherited
Base40ECE0 and therefore leaves every scalar at factory defaults; it does not
copy evaluator state. UV embedded assignment434940 is a separate path, not
evidence that FunctionEval's virtual clone deep-copies it.

The first clone scout lacked global clone-map initialization and faulted on
NULL+4; after supplying verified original52FD90(755588), clone/cleanup succeeds.
This was not a cap. The first CRT-floor fixture forgot its declared executable
guest page and faulted before the callback; adding that page fixes the fixture,
not an engine algorithm. No capped path was resumed or re-budgeted.

## Exact finite runtime observations

IAT6D9370 is MSVCR71.dll!floor. Only that finite library call is an explicit
math boundary; all function dispatch, x87 operations, RNG and clock mutation
execute original instructions. No time/OS/GPU API is forwarded.

For types other than0/7, native first adds delta to time, stores float32 time
but retains the wider sum; shifted=sum+XOffset is stored float32. It computes
whole=float32(floor(sum*frequency)*reciprocal), then
phase=float32((shifted-whole)*frequency). Note that XOffset is not inside floor.

| Raw type | Native scalar before amplitude/Y offset |
|---:|---|
| 0, 7 | return YOffset directly; no time advance or floor |
| 1 | sin(float32(2π) * phase), x87 FSIN |
| 2 | phase < .5 ? +1 : -1 |
| 3 | phase - .5 |
| 4 | .5 - phase |
| 5 | phase < .5 ? 2*phase-.5 : .5-2*(phase-.5) |
| 6 | (native RNG %10001) * float32(.0002) -1 |
| 8 | pitch*storedTime+YOffset; amplitude ignored |
| other | 0*amplitude+YOffset; time still advances |

Only1..5 wrap stored time, and only if time > reciprocal (strictly greater),
by subtracting whole once. Negative time is not normalized. Types6/8/unknown
accumulate without that wrap even though they still perform phase/floor work.
Type8 clamps when pitch<0 and value<limit, or pitch>0 and value≥limit;
**zero pitch never enters either clamp**.

Original413270/4132B0 is the624-word MT19937 dependency: state755658..756014,
index73FE8C, twist table73FE90, default marker625 seeds5489. The reconstructed
analytical RNG wrapper uses std::mt19937;630 successive original values match,
including the second twist. It is not asserted to be an original named class.
Random samples use modulo10001 and the original float32 coefficient, not a
host uniform_real_distribution. Runtime users share the original-style stream,
or explicitly pass an analytical test state.

## Serialization

`spFunctionEvalSerializer` ID1D2A151D, target9450E590, factory47ED20,
read47EE00, write47F220. Six established fields now have actual portable
shared-core read/write/index instead of only a grammar plan. Fields0..5:
type/frequency/amplitude/XOffset/YOffset/pitch. Unknown fields skip, repeated
fields overwrite; no pointer relationships.

Writer uses Fixed4 scalar headers and default suppression. Frequency/amplitude
use literal1 bit comparison in native; X/Y/pitch use floating comparison to0.
Negative-zero offsets are suppressed; observed quiet-NaN payloads survive.
Zero frequency is accepted by native reader and produces reciprocal +Inf;
the codec preserves it, while host runtime refuses nonfinite arithmetic.
Runtime time and clamp state are not serialized.

## Verification and limits

`pc-function-eval`: **21/21 fresh children**, **255 native assertions**,
21 exact native/source capture comparisons (including630 RNG words and scalar
value/time sequences); source suite **285/285**; build223/223; full
CTest **34/34,29.65s**. These are finite directed vectors, not proof of all80-bit
x87/non-default rounding cases. Decimal captures compare float32 values;
signed-zero result sign is not independently closed by JSON number equality.

Caps unchanged:100k instructions/2s call,30s child,64KiB arena,32KiB request.
Largest observed RNG call17,274 instructions; scalar fixtures use≤352 arena
bytes. Native factory, codec and cleanup are actual code; allocator/stream/CRT
boundary contracts remain explicit. No game launch or asset mutation.

Host bounds/RAII reject truncated/extra sections and unsafe runtime values;
native malformed EOF/partial mutations, allocation failures, all setter/reset
callers and exceptional floating inputs are not completely mapped. Full
UV/Trans/Color integration and whole-file/live renderer verification remain
connected work. No closed-class or completed-gate claim is made.
