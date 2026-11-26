# PC FunctionEval: scalar runtime and shared codec (checkpoint21)

## Identity, lifetime and storage

`spFunctionEval` ID9450E590 → `spEvaluator` E91D088D → BaseObject;
registration760B78/init6D3E10, getter478610, factory478840, vtable6EA9AC,
deleting destructor478660, virtual clone4788D0, Evaluate478680.
Actual factory allocates38 bytes; all tracked allocations are released.

| Offset | Observed role |
| --- | --- |
| 10 | runtime accumulated time |
| 14 / 18 | frequency / reciprocal |
| 1C | amplitude |
| 20 / 24 | X offset / Y offset |
| 28 | pitch |
| 2C / 30 | directional clamp limit / enabled byte |
| 34 | raw function type |

## Exact finite runtime observations

IAT6D9370 is MSVCR71.dll!floor. Only that finite library call is an explicit
math boundary; all function dispatch, x87 operations, RNG and clock mutation
execute original instructions. No time/OS/GPU API is forwarded.

For types other than0/7, native first adds delta to time, stores float32 time
but retains the wider sum; shifted=sum+XOffset is stored float32. It computes
whole=float32(floor(sum*frequency)*reciprocal), then
phase=float32((shifted-whole)*frequency). Note that XOffset is not inside floor.

| Raw type | Native scalar before amplitude/Y offset |
| ---: | --- |
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
Original413270/4132B0 is the624-word MT19937 dependency: state755658..756014, index73FE8C, twist table73FE90, default marker625 seeds5489. It is not asserted to be an original named class. Random samples use modulo10001 and the original float32 coefficient, not a host uniform_real_distribution. Runtime users share the original-style stream, or explicitly pass an analytical test state.

## Type 8 PC ramp arithmetic

The suffix at `0x004787BC` reads the binary32 time at `+0x10` after the
`FST` at `0x004786A2`. It multiplies that stored value by the pitch at
`+0x28` and adds the Y offset at `+0x24`. Both arithmetic operations use
64 significand bits and round to nearest, with ties to even, under `FPCW=037F`.
The result stays in x87 `ST0` through the return.

A nonzero byte at `+0x30` enables the directional clamp against the limit
at `+0x2C`. Positive pitch selects the limit when the ramp is greater than
or equal to it. Negative pitch selects the limit when the ramp is less
than or equal to it. The limit's raw zero sign is retained on equality. Either sign of zero pitch retains the computed ramp.

The analysis interface begins with the stored time word and returns the
ten little-endian bytes of `ST0`, including the sign of zero. It admits
normal binary32 values of magnitude at most `16384` and both zero encodings
for all four words. Time accumulation, the earlier phase calculation and
the other function branches belong to the full evaluator described above.
[Analysis interface](../../../Sparkplug/Code/Sparkplug/spFunctionEvalLinearRampPc.h)

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
