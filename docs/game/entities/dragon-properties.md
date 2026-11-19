# Dragon captured F32 properties (PC)

The own publisher for `wxDragonAIBehavior` binds `Walk Speed (cm/s)` to the word
at `+0x140` and `Backoff Speed (cm/s)` to the word at `+0x144`. Each selected
setter copies four unchanged argument bytes and returns with `RET 4`; each
getter executes `FLD` on its selected binary32 word. The analytical return is
the exact explicit ten-byte extended representation through the shared
`FromFloat` provider, including signed zero.

The facade accepts a supplied 328-byte prefix with captured primary
`0x00700150`. Both selected captured words and every selected argument must be
finite binary32 normals or signed zero. Other bytes remain opaque, including
arbitrary non-finite bit patterns outside the two selected fields. Writes stage
the complete prefix, permit input/output aliasing and change only the selected
four bytes. Wrong profiles, primaries, selectors or out-of-domain values leave
outputs unchanged; the default profile refuses.

The historical factory allocation of 388 bytes, selected access end of 328 and
360-byte guarded harness are distinct observations. They establish no current
native size or object layout. The typed primary, RTTI, clone-to-factory call,
registration and own publisher establish these selected owner bindings. The
Walk callbacks are shared by another class's Turn descriptor; matching callback
addresses alone do not establish an owner or physical inheritance.

The labels are preserved without adding unit conversion or AI movement rules.
Registration base IDs do not establish physical C++ inheritance. The current
factory entry remains protected and opaque, and the captured clone prefix does
not close its complete control-flow graph. Constructor, formal native
signatures, lifetime, production callers, runtime AI behavior, PS2, gameplay and
whole-class completeness remain unqualified.
