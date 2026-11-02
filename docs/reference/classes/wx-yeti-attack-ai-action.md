# wxYetiAttackAIAction

`wxYetiAttackAIAction` has class ID `30347478`, PC extent `0x494` and physical
[`wxAIAction`](wx-ai-action.md) prefix. Factory `5B2120` passes the allocated
receiver to `5B0980`; that constructor prefix forwards unchanged ECX to
`58FF00` at `5B09A0` before installing primary table `709418`. Getter
`5B0B30` returns record `767370`. The [PC layout](../../../Winx/Analysis/PC/wxYetiAttackAIActionAbi.h)
describes the captured allocation separately from portable C++ placement.

The [implementation](../../../Winx/Code/wxYetiAttackAIAction.h) provides the
complete Exit selected by primary cell `2C`: common body `5B79D0`. Its original
declaration owner remains unknown. Yeti's own typed table establishes the
consumer independently from Golem or Stormy.

## Captured command Exit

Exit captures command through `own20 → owner144 → actor130`, executes real
Reset `5992E0` through command table `703090` cell `04`, then clears own target
`3A8` at return `5B79E7`. The 38 native command stores precede this final
store. Complete raw own bytes `3AC..493` and represented parent state remain
unchanged. EAX is not given a portable Boolean meaning.

Source uses the existing
[`wxAICommandRecordForAnalysis`](../../../Winx/Code/wxAICommandRecordForAnalysis.h)
algorithm. A [structural host](../../../Winx/Analysis/Host/wxAIActionCommandExitHost.h)
shared by the new Yeti/Golem consumers resolves borrowed storage only. It
does not implement Reset or return operation success. The common record
retains unknown native class/RTTI/inheritance identity; its raw PC703090 tag
and default refusal follow the [command contract](../../game/ai/ai-command-reset.md).

## Admission and open behavior

The portable constructor binds explicit postconstruction target/raw232B state
and owner identity. Encoded pointers stay opaque and unowned. Default class
profile refuses; the explicit PC profile requires an empty inherited action
graph and live stable borrowed owner/actor/command records. Missing storage
or refused common Reset leaves the old target and raw tail unchanged. These
are analytical boundaries, not original null-fault handling or rollback.

Generic factory and Clone require unsupported context and throw `logic_error`;
Copy returns analytical false. Notify, Entry, active attack hooks and other
unqualified hooks refuse. Borrowed commands and targets survive portable
action destruction. Full native construction/destruction/owning graphs,
aliasing, concurrency, PS2, gameplay and whole-class completeness remain open.

The [dual driver](../../../Winx/Tests/wxCommandExitAIActionTests.cpp) retains
independent Yeti and Golem replays, including this full232B tail and real
shared Reset behavior under repeated and explicitly changed inputs.

## Own phase selected by primary `54`

The complete PC method `5B0160` branches on raw word `3B0`. For zero, it
captures owner `20`, clears own word `3BC`, then resolves that captured owner's
`144 → actor130` command and sets command byte `23` to one. Owner capture
precedes the own clear; the dependent owner/actor reads follow it.

For phase one, it captures own byte `3B8` before writing byte `3C0 = 1`.
When the captured flag is nonzero,
word `3E4` is a zero/nonzero sentinel: a nonzero value is preserved; zero
causes a read of existing global `755298` and its GameTimer word `1C`, followed
by `3E4 = current + 800` modulo `2^32`. It then clears byte `3B8`. Finally it
freshly resolves owner/actor/command and clears command byte `23`. This method
does not compare the deadline with the current time or test expiry.

Every other raw phase value clears words `3AC`, `3B0`, `3C4`, then sets bytes
`3C8` and `3B8` to one, in that order. This branch reads neither owner nor
command nor timer. All three exits are plain returns, without an inferred
Boolean result. Target `3A8`, remaining raw own bytes, parent state, complete
command records apart from byte `23`, and complete native Timer storage stay
unchanged within the method.

The source uses an independent default-unbound
[phase binding](../../../Winx/Analysis/Host/wxYetiAttackAIActionPhaseBindings.h).
Its owner resolver supplies structural borrowed storage; its Timer field is
an actual [`wxGameTimer`](wx-game-timer.md). Current time is the inherited
`spTaskTimer::GetStateForAnalysis().currentMilliseconds` stored field, not a
`Now` callback or fresh OS clock sample. Native GameTimer is `136` bytes with
a physical `spTaskTimer` prefix; its `1C` is distinct from the limited byte at
`1C` of the separate process `spTimer`. Portable state access does not assert
the native queue, pointer or multiple-interface layout on the C++ object.

Command byte `23` uses the common record's admitted raw storage API. That API
is our implementation detail, not a new native method or foreign class name;
default record profile and a different raw PC703090 tag refuse before the
command write. Existing Reset and the other captured command stores keep
their own contracts.

Phase admission is separate from Exit. It requires an explicit phase profile,
an empty inherited graph, and live stable borrowed inputs. Missing command
storage retains earlier own effects. A missing owner in phase zero retains
the `3BC` clear; a missing needed Timer in phase one retains `3C0 = 1` while
leaving deadline, flag and command intact. A later command refusal retains
the completed deadline/flag prefix. Other phases need no owner or Timer.
These analytical refusals do not model native faults or rollback. Cold Timer
creation, Timer queue/old encoded-pointer ownership, Entry's selector and
notification/resource continuations, other phases, PS2 and gameplay remain
open. Separate method profiles do not establish a native Entry/phase/Exit
lifecycle composition.
