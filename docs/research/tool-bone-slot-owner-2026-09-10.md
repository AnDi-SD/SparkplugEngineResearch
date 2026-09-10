# Importer: bone slots фактического владельца mesh

`SmoMeshReplacer.GetBoneSlots` теперь использует существующий
`SmoRenderableCatalog.TryGetStoredMeshOwner(...).Skin`. Прежний локальный поиск
Skin ancestor приписывал палитру физически вложенному mesh даже после более
позднего BaseMesh assignment к другому ресурсу; foreign entry с тем же индексом
тоже мог получить палитру чужого документа.

Общий compatibility lookup уже проверяет принадлежность entry документу и
фактическую связь содержащего Model/Skin с этим stored mesh. Проекция palette
index, Node ID и имени кости сохранена. Не менялись `ResolveBoneSlot` и его
default0, политика выбора owner среди reference-only/multiple consumers,
восстановленные игровые классы или Viewer.

Root выполнил Release build `SmoImporter.FormatTests`:9.58s,0 warnings/errors.
`--model-graph-links Media/SFX/vase.smo` прошёл3 полные замены и64 checks:
50 существующих плюс14 новых. Новые directed metadata fixtures проверяют
обычный Skin с двумя ожидаемыми slots, superseded BaseMesh, foreign entry,
reference-only Skin при другом физическом Model owner и неизменность входа.
Они не объявляются новыми original-game runtime fixtures.

Native DLL проверки:
`F422A92E84008E837D402B6EB6B621B743D4E2F49BD0CC7E543B0EDAE2F95028`.
Три выходных replacement SMO побайтно совпали с сохранённым
`tools-core-cycle-20260910-0700/model-graph-links`: проверены actual files и
соответствие SHA обоим report.json. Это сравнение существующих artifacts,
не повторный запуск проверки и не универсальное утверждение обо всех входах.

Build/run logs, report и byte comparison:
`local-data/results/tools-core-cycle-20260910-0730/bone-slot-owner/`.
[Manifest](../../research/tools-core-bone-slot-owner-block-2026-09-10.json)
фиксирует текущие sources/binaries/evidence. Изменены только Importer lookup и
существующий focused regression; общий engine code не изменялся.
