# Notification routing

The common [logical record](../../Sparkplug/Code/SparkBase/spNotification.h)
has eight fields: code, word04, word08, word0C, sender, word14, payload and tail.
Pointer-sized portable fields describe borrowed objects/data and do not form the
native 32-bit byte ABI. The record and every supplied pointer must remain valid
through the synchronous call.

Two recovered standalone operations build this frame. Their original C++
declaration owners remain unknown; neither operation introduces a native class.

| Operation | Routing and frame |
| --- | --- |
| PC `435D30`, `SendDirectedNotificationForAnalysis` | Sends to a supplied target through the notification host. word04/08/0C/14 are zero. A null target skips dispatch. |
| PC `40EC00`, [SendBroadcastNotificationForAnalysis](../../Sparkplug/Code/SparkBase/spBroadcastNotification.h) | Sets word0C to the supplied key, word04/08/14 to zero, and dispatches through the real shared subscription manager. |

Both operations copy code, sender, payload and tail from their inputs. They do
not acquire references, retain the frame, or own its sender and payload. The
broadcast helper captures the current `spSubscriptionManager` while preparing
the packet. If absent, native code creates and publishes a manager after all
eight words have been initialized. The portable
application supplies a required factory which constructs, publishes and retains
a real reconstructed manager. Missing or inconsistent providers produce a
separate analytical refusal; that result is not a native boolean return value.

Broadcast key zero is a subscription key like any other key. This differs from
the [message queue](../reference/classes/sp-message-queue.md), where word0C zero
selects direct dispatch through native word14. The immediate helpers leave
word14 zero and never reinterpret that field as their target.

The broadcast helper passes one mutable stack frame to the shared manager.
Subscribers run in that manager's ordered pointer-tree traversal. A subscriber's
changes to the borrowed frame are visible to later subscribers in the same
dispatch. The application must keep subscribers and their group stable while
dispatch runs. Changes to subscription membership, recursive manager dispatch,
provider failure, exceptions and whole application/hardware behavior remain
outside the qualified scope. PS2 equivalents are not inferred from this PC
helper. Portable manager pointers and native addresses have separate ordering
domains; mapped application fixtures must preserve the relevant receiver order.
