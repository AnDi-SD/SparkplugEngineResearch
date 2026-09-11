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
propagated. On success it calls `4C36A0` to find/build a platform Font and stores
the same borrowed pointer at `+0x28/+0x2C`. The common stage creates an owning
material graph at34/38, not font buffers. The platform Font producer is not
provided by the modern host: initialization now retains the completed material
prefix but returns false, with readiness false. Its former unconditional
success/readiness flags were incomplete reconstruction placeholders.
[Original geometry/material evidence,11 September](tool-text-geometry-2026-09-11.md).
