# `spDXInputManager`: DirectInput device owner

Registration `0x00764830` identifies class `0x10F20027`, direct base
`spInputManager / 0x55A1304D`, initializer `0x006D57C0`, and protected factory
entry `0x004C39D0`. No original TU/header string survives.

The leaf installs primary/support/device tables
`0x006F2688/0x006F2684/0x006F2668`. Getter is `0x004C3860`, deleting destructor
`0x004C3AB0`, body `0x004C3870`, and clone `0x004C3A60`. Device initialization
`0x004C3BB0` owns a DirectInput interface, keyboard, mouse and four controller
objects. Enumeration helper `0x004C3B20` writes controller count at `+0x3C`
and pointers at `+0x40..+0x4C`; destructor releases all of them. Consequently
`0x50` is a byte-exact observed extent, but it is not labelled exact `sizeof`
until the `.rld` allocation trampoline is resolved.

Device accessors expose mouse at full-object `+0x30`, controller by index at
`+0x40`, current count at `+0x3C`, and constant capacity `4`. Poll
`0x004C3920` updates enumerated controllers, then keyboard and mouse, returning
distinct native status values. The portable leaf preserves capacity, bounded
connectivity, initialization/shutdown and blank clone; COM creation/polling is
left behind an explicit unresolved platform boundary.
