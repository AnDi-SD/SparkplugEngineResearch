# `spPCFontManager`: PC font backend leaf

Exact original translation unit:
`Z:\Sparkplug\Code\SparkplugPC\spPCFontManager.cpp`.

The PC registration at `0x007647D0` identifies class `0x7B467097`, direct base
`spFontManager / 0x1640375E`, initializer `0x006D5790`, and factory
`0x004C35A0`. The factory allocates exactly `0x3C`, calls the common
constructor entry and installs primary/support vtables
`0x006F25C4/0x006F25C0`. The leaf adds no storage.

Vtable anchors are registration getter `0x004C3590`, destructor
`0x004C3620` and clone `0x004C3650`. Clone constructs a blank manager and uses
the inherited empty base-copy path, so fonts and backend state are not copied.

Leaf initialization `0x004C3830` first calls common `0x0041E8B0`; failure is
propagated. On success it calls the PC backend allocator/helper and stores the
same resulting pointer at `+0x28/+0x2C`. The portable reconstruction records
that second-stage transition without pretending to implement its unresolved
D3D resource type. Original header, helper name/type and buffer ownership
contract remain open.
