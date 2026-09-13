# PC scene boundaries and real material payload

## Begin/EndScene

PC renderer secondary interface `complete+18`, slots3/4:

- **4BB950** calls device `+A4` first, then writes complete `+CBC0=1` and
  `+4C=0`; returns AL1 regardless of HRESULT. Does not increment counter40.
- **4BB9D0** calls device `+A8` first, then increments complete `+40` modulo
  uint32 and clears `+CBC0`; returns AL1 regardless of HRESULT. Does not reset4C.

Duplicate Begin or End still invokes COM; duplicate End increments twice. A declared nonboolean initial active byte7 is normalized only by the relevant post-call store. COM observers record the **old state**, proving call/cache ordering.

Portable `spDXRenderer::SceneStateForAnalysis` exposes just these three known
fields to static begin/end methods; it does not invent the full native class
layout or constructor defaults. `resetWord4C` keeps an offset-based name because
its broader counter meaning/original spelling is not proved. NULL device callback
is a host-only rejection guard; HRESULT handling follows the actual wrapper.
