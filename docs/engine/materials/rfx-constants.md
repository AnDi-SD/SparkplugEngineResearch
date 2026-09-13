# PC RFX constants and pass persistence

## Parameter descriptors

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

`RmDirectXEffect`, `RmStringVariable`, `RmMatrixVariable`, `RmState`,
`RmTextureReference`, and an unknown tag do not create variables/parameters
in the observed XML engine dispatch. RmStringVariable's ID is handled by the
separate file-loader regex path, which remains open here.

Open: variables and their ownership, loader regex/name/ID, source generation,
compiler/reflection, malformed inputs and native error objects. The new
constant producer is not yet connected to a complete generating shader miss.
