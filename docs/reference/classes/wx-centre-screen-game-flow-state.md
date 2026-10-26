# `wxCentreScreenGameFlowState`

`wxCentreScreenGameFlowState` has ClassID `63CF578D` and a physical
`wxGameFlowState` prefix at offset `0` on PC and PS2. Its native extent is `48`;
all offsets and native constants below are hexadecimal. The recovered portable
class implements the two material ownership hooks at PC slots `20` and `24`.
Its parent constructor and lifetime use the qualified PC parent contract.

The native constructor leaves words `3C`, `40`, and `44` unwritten. Portable
storage tracks the initialization of each slot independently. A stored null in
the host object is not presented as a native default. Entry overwrites all three
slots as it progresses. Exit requires an explicitly known `3C`; a cold Exit with
an unwritten pointer is outside the safe analytical domain.

| Field | Meaning in the recovered hooks |
| --- | --- |
| `3C` | Raw material pointer, with its own native 16-bit reference edge |
| `40` | Raw copy of the current application owner's word `64` |
| `44` | Raw copy of a freshly resolved application owner's word `68` |

The semantic names and interpretation of the last two words remain unknown.
They are copied as 32-bit words, including arbitrary bit patterns.

Entry performs these operations in order:

1. Creates a material, stores its raw address in `3C`, and increments the
   material's unsigned 16-bit reference word at `8`.
2. Stores render override byte `1`, render state words `8=2`, `7=2`, and `3=0`.
3. Creates a real `spMaterialPassLayer`, reloads the current material in `3C`,
   and installs the pass at that material's current pass count.
4. Stores pass final blend operation `2`, creates a real `spStdLayer`, and
   installs it at the pass's layer count read after the layer factory returns.
   The standard layer owns its real `spMaterialTexture` payload.
5. Resolves the application owner and copies word `64` to `40`. It resolves
   the owner again and copies word `68` to `44`.

The material render state array starts at PC `18` and PS2 `20`; thus these
three words occupy PC `38/34/24` and PS2 `40/3C/2C`. The PS2 compiler moves the
same literal integer bit patterns through floating registers. This does not
change them into floating-point values `2.0` or `0.0`.

PC creates `spDXMaterial` through `4A9460`. PS2 calls the allocating
`spPS2Material` constructor at `1F2A90`, whose inputs include static color data;
this is distinct from registered factory `1F2FC0`. The portable host supplies
these foreign factories with real reconstructed material/pass/layer objects.
Material and pass/layer manipulation reuse the existing Sparkplug sources.
The broader factory color/default behavior is not inferred from these hooks.

Exit reads the known pointer at `3C`. For a nonnull material it decrements the
unsigned 16-bit reference word, invokes deleting virtual destruction with flag
`1` only when the result is zero, and then clears `3C`. The clear follows the
deleting callback even if that callback changes the state's pointer. Exit then
resolves the window and clears its 32-bit word `1294`. Both arithmetic operations
wrap modulo `10000`; a decrement from zero becomes `FFFF` and does not delete.

Repeated Entry overwrites the previous material pointer without releasing its
reference. The state destructor also leaves `3C` untouched and performs only
the parent's cleanup. These behaviors are preserved. The host keeps material
allocations in an external arena and exposes their native reference words
separately from C++ ownership. It must not infer a release from raw pointer
replacement or from state destruction. Pass references use the existing shared
owner representation; pass-to-layer and layer-to-texture edges are direct owners.

Copy is the inherited literal no-op: an existing destination keeps all its own
slots, timer, and host binding. Clone constructs a fresh state with a fresh
parent timer, registers the source/clone pair, and applies that no-op Copy.
The fresh clone's three own words therefore remain unwritten.

The host interfaces are required synchronous dependencies. Factory and delete
callbacks can change state fields or external globals; the hook preserves the
captured pass and the explicit material/application/window reloads described
above. Field borrowing exposes storage rather than implementing an unknown
foreign operation. Null dependencies, capacity overflow, unknown pointer input,
allocation failure, and exception propagation do not claim native behavior.

The substantial GUI/FPU body in native slot `30` remains unrecovered. The
portable boundary explicitly refuses it rather than returning the parent's
successful no-op result. PC bounded Entry/Exit and their material ownership
paths are qualified separately from PS2 full Exit returns and fresh foreign-call
prefixes. PS2 constructor/heap/lifetime, stopped foreign calls, rendering,
hardware behavior, and whole-class/gameplay completeness remain open.
