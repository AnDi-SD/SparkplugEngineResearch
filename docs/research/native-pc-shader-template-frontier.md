# PC shader template frontier and scope v2 correction

Observed during CP49, 7 September2026. PC EXE SHA256
`3F022480BF55045DA4BF692E4BC8862ED38FC024E8A964A558FBDFDF646DFC4F`.
These are **registered engine classes**, absent from workflow-v1. They must
remain required/unrated until explicitly assessed; they are not new invented
helpers and their existence is not behavior completeness.

| Class | Class ID | Registration / init | Confirmed connection |
|---|---|---|---|
| spPCEffectTemplate | 30E058FF | 765558 /6D5EB0 | 4CFFE0 producer path, actual TU string6F3A18 |
| spPCRFXFileLoader | 01A95832 | 765618 /6D5F10 | manager4C97E0→4D5410/4D56D0 reads Fixed.rfx |
| spParser | 3EC20087 | 7655B8 /6D5EE0 | registered direct base of PCRFXFileLoader |

PCEffectTemplate's registered direct base is spBaseObject, **not** an assumed
spDXEffectTemplate. Registration has NULL factory and NULL property callback;
vtable6F3A64 contains clone slot4A1BF0, copy40ECE0, RTTI4D0950. Actual class
name at6F3A80; source path is
`Z:\Sparkplug\Code\SparkplugPC\spPCEffectTemplate.cpp`.
Parser registration factory4D0DA0; PCRFXFileLoader factory4D54B0. These factories
and complete template/parser lifetime were **not executed or reconstructed**
in CP49. Catalog independently records all three PC-only, no PS2 membership.

Manager4C97E0 calls base file scan4B0890, builds `%sShaders\` and
`%sFixed.rfx`, constructs loader4D5410 on its stack, calls4D56D0 and stores
the result in manager40. Non-NULL result is passed to4D0650; loader cleaned
through4C9790. 4D56D0 reads text via4D0A30 before parser processing.
No whole parser/compiler execution is substituted by a fabricated result.

4CFFE0 has separate611241 and611324 branches. The latter returns bytecode,
error and constant-table interfaces; the table feeds original shader append
4AF940. Bytecode size/data are then copied to shader48/4C. Exact upstream
argument semantics, library attribution, format grammar, template ownership,
all success/failure cleanup and cache-miss end-to-end execution remain open.
These are the next connected research tasks, before claiming native rendering.

## Accounting change, not research gain

Workflow-v2 retains **every** v1 class and adds only these three confirmed
PC dependencies: union308→311, PC276→279, PS2245 unchanged. Adds an explicit
non-RTTI obligation for shader compiler/reflection boundary contracts; third-party
compiler implementation is still excluded by the unchanged scope policy.
All7PC gates remain0passed/6partial/1open. New dependency classes get no
automatic score. Historical v1 and its immutable manifests remain unchanged.
Reports must state both comparable v1 research growth and current v2 percentage.
