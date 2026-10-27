# spProjectionManager and spPCProjectionManager

The PC `spProjectionManager` (`24A010DA`) physically extends `spCrossPlatform`,
which preserves the `spNamedObject` name prefix. The concrete PC descendant is
`spPCProjectionManager` (`E10D6FC2`). Both native PC extents are `0x24` bytes.
The base registration has no factory and its Clone returns null. The concrete
factory constructs a fresh disabled manager with an empty borrowed list.

| Native PC field | Meaning |
| --- | --- |
| `00..13` | CrossPlatform/NamedObject prefix; shared name at `10` |
| `14` | Raw enabled byte, initially zero |
| `15..17` | Unwritten padding |
| `18` | Native list allocator state, not initialized as zero data |
| `1C`, `20` | Owned list sentinel and count; each payload is borrowed |

The portable classes have their own C++ layout. Their `std::list` storage is a
technical replacement for native list allocations; it does not reproduce the
native allocator ABI. List insertion scans for an equal pointer before append.
Null pointers are retained by that storage operation and duplicate insertion is
suppressed. Removal deletes every matching list node. Neither operation changes
the payload's intrusive references. Destruction releases list storage and the
inherited name, without destroying the borrowed projections or their helpers.

Copy is the inherited NamedObject operation. It first releases and clears the
destination name, then rereads the source name and shares it when present. A
self-copy therefore clears the name. The destination's enabled byte and list
are preserved. Concrete Clone constructs an empty, disabled concrete manager,
registers the source/clone pair and invokes that Copy operation. It transfers
neither list entries nor the enabled state. Final-owner name-pool cleanup is a
separate boundary from the qualified retained-name copy behavior.

The enable operation writes the literal byte one and returns true. The separate
byte setter stores the low eight bits of its argument, and the getter returns
the raw byte. The two first draw phases return true without changing state.
The initialization and late draw loops do not inspect the enabled byte; the
native Scene caller performs that gate separately.

Initialization visits list entries in order and directly executes standalone
PC helper `4243D0`. With an already bound helper in projection field `94`, this
operation reloads `94`, writes the projection pointer into helper member `18`,
reloads `94` again, and calls that helper's virtual slot zero. It returns the
literal true regardless of the foreign helper's return value. The final reload
matters when a borrowed field aliases or is replaced during the owner write.
Null `94` requires the native helper factory; this reconstruction explicitly
refuses that unqualified acquisition path. The source declaration owner of
`4243D0` has not been recovered, so the operation remains standalone.

Late draw calls every borrowed projection's virtual `38` with the captured
camera argument. Both loops initialize an eight-bit accumulator to one and
combine every returned low byte with bitwise AND. They never short circuit.
Consequently draw results `1,2,3` produce zero even though every result is
nonzero; high bits above the low byte do not contribute.

Traversal follows the live list after each callback. Appending can extend the
current pass, and removing a later entry can skip it. Independent synchronous
reentrant calls retain separate traversal frames. A portable guard detects
erasure of the current list node before incrementing its invalid iterator. It
preserves completed effects and refuses rather than reporting a native return.
Whole-list fixture replacement during traversal is also refused. Additional
analytical limits stop a pass after 65,536 visits or 128 nested active frames.
The native loops have no corresponding limits.

Projection/FX handles, fields and virtual effects remain required foreign
dependencies. Their lifetime must span the synchronous operation. Native helper
acquisition, Projection drawing, materials, renderer/device work, allocation
failure behavior and full Scene initialization remain separate boundaries.
PS2 methods are not qualified by the PC implementation or by shared class names.
