# spEditBox

`spEditBox` (`253C432D`) physically extends [spWidget](sp-widget.md). The native
PC factory creates `0x80` bytes; its table binds its own Copy, Clone, Notify and
selection-refresh operations. The portable class has a separate C++ layout.
The recovered implementation currently qualifies bounded PC string, property,
Copy/Clone, text/cursor, selection, append/delete and selected Notify behavior.
It does not qualify the whole class or transfer PC behavior to PS2.

| PC member | Meaning and ownership |
| --- | --- |
| `54`, `58`, `5C` | Borrowed NORMAL, DISABLED, HIGHLIGHTED TextNodes |
| `60`, `64` | Borrowed current TextNode and cursor Node |
| `68`, `6C`, `70` | Owned current byte buffer, allocated byte count, logical length |
| `74` | Maximum measured text extent, initially zero |
| `78` | Owned optional initial byte string |
| `7C` | Raw override-property byte, initially zero |
| `7D`, `7E` | Password character followed by a zero terminator |
| `7F` | Unwritten padding, excluded from portable state |

Current and initial strings differ. An empty initial string releases the old
string and stores null. A fresh nonnull empty current string creates a two-byte
buffer; a nonempty current string grows its buffer to `strlen + 1` when required.
Shorter and empty replacements retain the existing allocated count. A current
setter argument equal to its nonnull current buffer pointer skips the byte copy
and still updates TextNodes. The factory null/null case creates an empty
two-byte current buffer and performs that same update. Byte strings
stop at their first zero; their encoding is not inferred from this operation.

Initial replacement releases the old initial string before inspecting its
input. The portable analytical API refuses input aliases into that released
allocation. This also leaves populated initial-string self-Copy outside the
qualified lifetime domain. Stable terminated inputs are required; a portable
65,535-byte extent limit is an analytical refusal. Allocation failure and native
allocator ABI are not modeled by the `std::string`/`std::vector` storage.

Copy first executes Widget Copy. It shallow-copies all five borrowed TextNode/
cursor pointers, replaces the destination's initial string, replaces its
current string through the actual text-update path, then copies maximum extent.
The destination's override and password bytes are preserved. Clone creates a
fresh EditBox, registers the mapping and runs Copy; those two properties retain
their fresh zero defaults. Current and initial strings are separately owned by
the resulting objects. Destruction releases current then initial storage before
the inherited Widget/GUI destruction; the borrowed text and cursor nodes are
not destroyed by those fields.

Text update visits NORMAL, HIGHLIGHTED, DISABLED in that order, retaining repeated
node pointers. Each cached TextRenderable receives an owning text copy and real
layout update, followed by the real RenderNode bounds rebuild. This uses the
existing shared Text/Font implementation. Nonempty layout requires established
FontManager state; it does not stand in for cold manager startup.

Selection refresh first runs Widget refresh, hides the previous current text
node, reads the resulting state index, selects that text-state node and enables
it. The cursor is enabled only when both highlight and enabled bytes are nonzero.
Cursor placement retains cursor Y, takes current TextNode X/Z, and measures the
cached Text's string through its directly bound Font. It adds the scaled width
for alignment zero, half the scaled width for alignment one, and no width for
other alignments. It stores the cursor position and marks its local transform
dirty. A nonempty direct measurement needs the cached Text's actual Font;
the layout manager's fallback font does not replace this separate dependency.
Missing cached Text or a null cached string skips width multiplication and
retains literal positive zero. An existing empty string still multiplies its
measured zero by the scale, preserving the different signed-zero result.
Direct measurements also refuse strings above 65,535 bytes. With eight-bit
glyph advances, their unwrapped width is below `2^24`, so multiplying that
integer width by a finite binary32 scale is exact in binary64 before the
native-equivalent binary32 spill. The admitted cursor numeric domain uses
finite positions/scales; arbitrary nonfinite payloads and floating exceptions
are not qualified by the current small-font comparisons.

Append uses the password byte when nonzero, otherwise the incoming low byte.
It reuses spare current-buffer capacity or builds a replacement string. Its
native growth path truncates a previous logical length above 255 before adding
the byte. After updating text it measures against member `74`; a positive limit
exceeded by the scaled width removes the appended byte and updates again.
Delete removes one logical byte when present. These operations are byte based,
and the logical length is retained separately from the first-zero string length.

Own Notify preserves its separate dispatch. Enabled `0E`, `10`, `11` and `12`
forward directly to the live external owner; the latter two do not perform
Widget's highlight change. `13` appends its payload byte. `14` with payload
`0D` deletes a byte and then continues through Widget Notify. `16`/`17` change
highlight, perform the own selection refresh and continue through Widget.
Ordinary unhandled codes use the inherited Widget route. Disabled own routes
return without those effects.

Enabled `14` Escape/`54` reads the actual GUIManager singleton and compares
its borrowed focus pointer with this EditBox. When equal it directly clears
focus through the shared manager, including the real old-receiver `17`
notification and selection refresh; then it continues through Widget Notify.
When another receiver or null has focus, it proceeds to Widget without changing
focus. A missing singleton and the manager's analytical nesting refusal remain
explicit dependency refusals; completed callback effects are retained.

Own `1C` returns immediately for a null root. For an existing stable root it
looks up `NORMAL`, `DISABLED` and `HIGHLIGHTED` in order through the actual
recursive substring Node search. Normal and highlighted collision groups
become `4`; disabled groups are preserved. Disabled and highlighted state
nodes are hidden recursively. The selected GUI pointer becomes NORMAL while
the previous state index is retained. This own route returns without the
inherited GUI enabled-setter side effects.

The qualified binding profile has an existing, typed `NORMAL_TEXT` TextNode.
Its cached string is measured through its bound Font. Center alignment shifts
the node by negative half the scaled width, right alignment by the negative
scaled width, and other alignments by zero. The observed `4365E0` primitive
adds that local vector and marks Node state dirty; its original source owner
is unknown. Cached Text alignment then becomes zero and layout is rebuilt.
Existing typed `DISABLED_TEXT` and `HIGHLIGHTED_TEXT` receive alignment zero
and the normal node position. They are hidden, current text becomes NORMAL_TEXT,
and the found `CARET` node is hidden. Initial text takes precedence; otherwise
an unset override byte selects the normal cached string, and a set byte selects
null. The real current-text setter performs the final copy/layout/cursor work.
Repeated binding observes the alignment and cached text produced by the first.

Binding borrows the resolved nodes and preserves input names, links and
ownership. The tested graph uses finite positions/scales and the bounded Font
measurement domain above. Native creation, naming and attachment of a missing
or wrong-type NORMAL_TEXT remain open; source refuses at that branch after the
completed GUI prefix. Arbitrary lookup callbacks and graph mutation require
separate qualification.

A freshly empty normal cached Text leaves its maximum bound unassigned: the
original empty-layout path writes the minimum twice. Its known minimum/sphere
and binding effects remain represented; the portable optional maximum stays
unknown. Allocator bytes and derived bounds that consume that unknown value
are outside the binding comparison. Existing maxima from a previous nonempty
layout are retained when the string becomes empty.

Enabled `0F` directly sets this EditBox as the real GUIManager focus. After
the old/new focus notifications finish, it reloads the external owner pointer
and forwards the original mutable notification frame. Enabled is not checked
again. Owner changes or disabling during focus callbacks therefore affect the
fresh owner selection without suppressing dispatch. A changed message code is
consumed by the owner and is not rerouted through Widget. The admitted owner
profile uses live typed `spGUIObject` receivers and their real Notify body;
production of the owner relationship and arbitrary owner overrides stay open.
Missing manager/owner and nesting-limit paths explicitly refuse, preserving
completed focus effects. A disabled `0F` returns before either dependency.

`SetTextGraphForFixture` supplies already resolved typed nodes directly,
without ownership or a graph producer.
The external GUI host retains its notification-target and node-effect
responsibilities. Missing measurement dependencies refuse after completed
effects. Cold text-node creation, GUI/input startup, property registration,
graph destruction, native failure behavior and hardware remain separate gates.
