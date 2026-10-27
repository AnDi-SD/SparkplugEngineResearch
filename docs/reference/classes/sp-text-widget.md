# spTextWidget

`spTextWidget` (`0x0A3053A8`) derives from `spWidget` (`0x7CB52800`) in the
PC and PS2 registration graphs. The PC object occupies `0x54` bytes, exactly
the observed Widget extent; its constructor adds no own state. The portable
[class](../../../Sparkplug/Code/Sparkplug/spTextWidget.h) uses the existing
Widget, GUIObject, Entity and Node implementations. Its method names and
declaration path are analytical.

PC properties operate on actual `spTextNode` objects and their cached
`spTextRenderable`. Text and colour getters first test the Entity's bound root.
If that root is a TextNode, it takes precedence even when its cached Text is
absent. Otherwise the getter tests only the NORMAL state node. A missing Text
returns a null string pointer or the colour `0xFF000000`. The separate
normal-colour getter always tests NORMAL and ignores the root.

Text and colour setters likewise select the typed root exclusively. When the
root is absent or has another type, they visit NORMAL, DISABLED and HIGHLIGHTED
in that order, skipping missing or differently typed nodes. Repeated node
pointers remain repeated visits. Setting text copies the byte string through
the shared TextRenderable setter, rebuilds its layout using the established
FontManager, and then rebuilds the TextNode's renderable bounds. Setting colour
stores the raw colour word without rebuilding layout or bounds.

Copy first executes the Widget copy. The destination retains its Entity root
and visible byte; state-node pointers are borrowed and copied shallowly. It
then reads the source text, applies the destination text setter, reads the
source colour and applies the destination colour setter. Consequently a copy
can update the shared state nodes or the destination's existing text root.
Clone creates a fresh TextWidget, registers the clone mapping and executes the
same copy operation. Notifications, selection, Node binding and GUI teardown
remain the inherited Widget and GUIObject operations.

The current implementation covers the bounded PC property and copy domain
over stable, correctly typed Node graphs. It requires the shared FontManager
for nonempty layout. Native platform-manager acquisition, property-system
registration, allocation failures, full GUI startup, rendered behaviour and
PS2 property execution remain outside this domain. A null string with a live
Text target is rejected by the shared analytical setter; it is not a native
successful-input contract. The source does not claim the native pointer ABI
or whole-class completeness.
