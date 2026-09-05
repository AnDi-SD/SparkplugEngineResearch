# `spPS2FontManager`: stateless PS2 font backend leaf

No original source-path string survives; `Code/SparkplugPS2` is inferred from
the platform/module pairing.

Registration `0x004B72C0`, initialized at `0x00485010`, identifies class
`0x31650C4A` and direct base `spFontManager / 0x1640375E`. Factory
`0x001EEDA0` allocates exactly `0x38`, calls common constructor `0x00168AC0`
and installs vtables `0x004913A0/0x004913C4`; therefore the leaf adds no
storage. Getter is `0x001EEBD0`, destructor `0x001EEBE0`, clone
`0x001EEC50`, and support destructor thunk `0x001EEE10`.

Unlike PC, the PS2 leaf does not replace the common initialization target and
has no extra backend field. Its clone still constructs empty runtime state and
routes copy through the inherited no-payload path. Reconstruction preserves
that distinction and is covered by RTTI/factory/clone tests. Original
TU/header and platform-specific reason for the absent override remain open.
