# wxMosquitoAttackState

`wxMosquitoAttackState` is a `wxCharacterState` with class ID `6CF96918`,
native size `0x3C` on PC and PS2 and state selector `3`. It adds no observed
instance fields. Factory and clone create the base defaults; inherited Copy
does not copy live state. Inherited Reset restores flags `1C/1D/1E`, clears the
pending animation and reset fields, and preserves flags `1F/20`.

Entry changes the request key to `(key & F0000401) | 401`, resolves an animation,
queues it with mode `0` and interrupt enabled, stores the returned handle, then
dispatches virtual update. Queue runs even for a null handle or the same handle
already pending. Entry does not release the previous pending animation first.

Update clears the owner's direct action-control word, reloads the owner and
resets its entity controller. PC calls the external reset service through
`owner+124 -> entity+12C`. PS2 follows `owner+130 -> entity+138` and writes zero
to controller words `1C8/1CC/1D0`, `1BC/1C0/1C4`, `1D4/1D8/1DC`,
`1A0/1A4/1A8` and `194/198/19C`.

Permission slot `34` always queries the completion consumer with consume flag
`true`, including a null pending handle. Slot `38` returns false. The event
hook compares the whole case-sensitive name `event_shoot`. A match invokes the
unnamed entity operation with argument zero: PC `entity+140` virtual slot `38`,
PS2 `entity+14C` virtual slot `40`. Other names leave the entity untouched.

The portable class restores these own methods. Animation resolution, playback,
borrowed entity graphs, controller reset and the unnamed entity operation
require an explicit host. This component does not establish complete live-game
integration or the meaning of that unnamed entity operation.
