# `spPS2InputManager`: PS2 pad aggregation

Registration `0x004B76A0`, initializer `0x00485150`, class ID `0x462B48E1`
and base `spInputManager / 0x55A1304D` are direct executable facts. Factory
`0x001F1370` allocates exactly `0x48`, calls common constructor `0x0016D8F0`,
installs primary/support/device tables
`0x00491560/0x00491584/0x00491590`, and clears `+0x40/+0x44`.

Initialization `0x001F10B0` creates two controller objects at `+0x20/+0x24`
and two auxiliary devices at `+0x40/+0x44`, then sets common ready byte
`+0x1C`. Shutdown `0x001F0FF0` releases all four and clears that byte.
Destructor is `0x001F1170`, clone `0x001F1200`, registration getter
`0x001F0D80`. The device table contains 14 operations; the analyzed aggregation
path `0x001F0E30` translates pad results into a combined bit mask.

Portable reconstruction preserves the exact two-slot capacity, lifecycle,
bounded connected-count view and blank clone. It intentionally does not claim
names for the 14 operations or emulate Sony pad/auxiliary SDK objects. Original
TU/header, exact state block `+0x28..+0x3F`, button enum and controller class
identity remain open.
