# wxOpenSecretPassageState

`wxOpenSecretPassageState` is a `wxCharacterState` with class ID `0A9B5545`,
native size `0x3C` on PC and PS2 and state selector `41`. It adds no observed
instance fields. Factory and clone use the base defaults; inherited Copy leaves
live state unchanged. Inherited Reset restores only flags `1C/1D/1E`, clears
the pending animation and reset fields, and preserves flags `1F/20`.

Entry changes the request key to `(key & F0000D00) | D00`, resolves an animation,
queues it with mode `0` and interrupt enabled, stores the handle, and clears
the owner's direct action-control word. Queue runs even for a null or unchanged
handle, without first releasing the previous pending animation. The final
control write is direct; entry does not dispatch virtual update. Own update
performs the same direct control clear without changing the request.

Permission slot `34` returns true immediately for a null pending handle.
Otherwise it queries the completion consumer with consume flag `true`.
Slot `38` returns false.

The event hook compares the whole case-sensitive name `SND_INTERACTION`.
A match reads the borrowed entity pointer from PC `owner+124` or PS2
`owner+130`, then sends the filtered packet
`[2762, 0, 0, 6, sourceState, 0, entity, 0]`. The entity may be null. Other
names send no packet. The meaning of filter `6` remains unqualified.

The portable class restores these own methods. Animation lookup and playback,
borrowed event/entity graphs and notification dispatch require an explicit
host; its reuse of the OpenGate host contract is an analytical adapter choice.
This component does not establish complete live-game integration.
