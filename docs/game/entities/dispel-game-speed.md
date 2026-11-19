# Game Speed descriptor in DispelGame

The PC descriptor named `Game Speed (RPS)` binds a setter that ignores its
argument and a getter that loads an absolute binary32 word. In the qualified
pristine executable, the word at `006DBA9C` is positive zero in a read-only
`.rdata` section. The setter reads its return address, does not read its
argument or receiver, and performs no store. The getter loads the constant
and returns through the x87 stack.

The stateless `wxDispelGame` analysis facade represents only this qualified
constant-image operation. It has no captured receiver prefix or physical
inheritance declaration. The admitted profile returns ten physical x87
payload bytes, all zero, and accepts every 32-bit setter argument unchanged
by interpretation. Default refusal preserves getter output.

The registered base ID identifies `wxEntity` in the registration system;
it does not establish physical C++ inheritance. Historical factory allocation,
current object layout, native C++ signatures, production caller behavior,
live-global mutability, lifecycle, PS2 behavior and whole-class readiness
remain unqualified. The descriptor label alone does not establish gameplay
speed or its units.
