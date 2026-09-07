# PC full Skin material state protocol (CP77)

2026-09-07; same pinned pristine PC executable as [CP76](native-pc-skin-owned-material-render.md).
Тот же whole-loaded Skin/material/Node, real bbush SAN/scene и decoded mesh.
Material6C runtime input1, rendererC1C4 input7B, shared7400FC input33.

Whole46A240→423FD0 выполняет pre callback до сохранения C1C4 в7400FC и
обнуления C1C4. World SetTransform/device видят0.4240D0 восстанавливает7B
до post callback. Pre false оставляет7B/33 и пропускает всё; post false
возвращает0, но byte уже восстановлен, active bone count остаётся1.
Полный успех и отрицательные device HRESULT возвращают1/count0.

RendererC188 input1 сохраняет прежние C18C и C194 (отдельный белый fallback
и10203040), хотя Skin20 содержит цветной материал и Skin28=80402010.
Material6C по-прежнему управляет временным byte независимо от выбора.
Shader key при таком fallback снова00020011; source/native constants и
device traces совпадают. Shared save slot передаётся source явно указателем;
это не stack restoration. Прежнее native interleaving overwrite evidence
описано в [renderer protocol](native-pc-renderer-protocol.md).

**5 exact captures,346 native assertions**: restore,pre-false,post-false,
failed-device,preserve-selection. Четыре по62+9, pre57+5. Read70998,
render48..15305,peak64656,103 owner generations released в каждом случае.
CP76 regression4/4 успешна. Source guards теперь допускают material6C при
наличии shared save slot, C188 при валидном selected material; queued
transparency и non-NULL fog пока не закрыты. GPU/OS/caps не изменены.

```powershell
python research/native_workbench.py run pc-skin-material-protocol --deadline-utc 2026-09-07T16:00:00Z
```
