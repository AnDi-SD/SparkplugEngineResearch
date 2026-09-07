# PC RFX constants and pass persistence — CP53

Pristine PC executable SHA256
`3F022480BF55045DA4BF692E4BC8862ED38FC024E8A964A558FBDFDF646DFC4F`.
Extends [CP52](native-class-sp-pc-effect-template.md); PC only.

`4D4750` is a cdecl XML-library start callback, passes argument4 to
`4D3A80` with argument1 as loader. End `4D6050` is also cdecl. Supplying the
wrong callee-pop contract initially produced a misleading `instruction/time
cap` message although EIP was already `32000000` (normal return). Static RET
and observed stack established the correct cdecl contract; no capped guest
execution was resumed. The harness now reports completed-call stack mismatches
separately, preserving every instruction/time limit.

## Parameter descriptors

`RmShaderConstant` branch `4D44B1..4D4572` uses loader `+724`, the active
shader pointer last selected by `RmShader` or `RmHLSLShader`. Missing active
pointer takes an error branch (not reconstructed by CP53). Missing NAME or
REGISTER returns without appending. Otherwise the original copies NAME into
a32-byte stack field, calls imported MSVCR71 `atoi` at IAT `6D930C`, sets
register count1 and calls original vector append `4AF850` with a44-byte record.
The descriptor's type word `+20` is not initialized here; it is deliberately
excluded from semantic capture rather than assigned a guessed type.

Reconstruction preserves the bounded signed-decimal-prefix conversion,
including invalid/empty input ->0, trailing text, negative value represented
as uint32, and signed32 limits. CRT overflow is explicitly outside the current
contract. Names longer than31 characters would overrun the original stack
field; source reports analytical incomplete and uses safe string storage.

`4CFF40` pass reset clears shader strings/version/assembly, but leaves its
parameter vector intact. Active loader pointer `+724` also remains selected.
Filled tests prove both facts. A vertex parameter from pass1 is copied into
pass2 even if only a pixel shader is subsequently defined; pixel parameters
are accumulated independently. Original destructor frees all copied vectors.

Repeated HLSL definitions preserve absent strings and preserve version bytes
for unknown targets. Switching to assembly changes code only if supplied,
sets bytes1,1,1, and retains HLSL declaration/entry/target strings.

## Other observed callbacks

`RmStreamChannel` with exact `USAGE="6"` ORs bit0 into template word `+40`.
Other spellings such as `06` do nothing. Its constructor leaves that word
uninitialized: portable source carries value plus known-bit mask, and the
differential fixture supplies explicit initial bits `A5A50020` on both sides.

`RmDirectXEffect`, `RmStringVariable`, `RmMatrixVariable`, `RmState`,
`RmTextureReference`, and an unknown tag do not create variables/parameters
in the observed XML engine dispatch. RmStringVariable's ID is handled by the
separate file-loader regex path, which remains open here.

## Evidence and limits

Eight event sequences compare every post-event active pointer, initialized
flags, current two-shader pass and accumulated pass vector (including complete
semantic parameters). The explicit fixture replaces only the XML-library
attribute query `6B1720`, whose +4 value record contract was exercised by
CP52's full native XML chain. All engine callbacks execute original code.
Maximum per event is22079 instructions.

A ninth independent test executes the **whole original small XML parse** with
one assembly shader and constant (`xml-constant`,67583 max instructions),
including repeat initialization and complete teardown, and compares it to C++
fed externally decoded XML. This validates the event-boundary result against
the integrated original library path. `pc-rfx-events`:9/9 children,9 exact
captures,59 native assertions; C++ effect suite45 assertions. CP52 regression
is retained. No OS/GPU call, limit increase or full RFX file closure.

Open: variables and their ownership, loader regex/name/ID, source generation,
compiler/reflection, malformed inputs and native error objects. The new
constant producer is not yet connected to a complete generating shader miss.
