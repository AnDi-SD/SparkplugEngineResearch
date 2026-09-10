# PC renderer: подтверждённый default-material producer

Fresh original factory `4C5AB0` завершился за 4,780,619 инструкций / 16.32s.
`C9C0` получил настоящий DXMaterial с одним pass и двумя StdLayer. Прежняя
граница происхождения fallback material закрыта для этой исходной PC цепочки.
Device/startup/game-loop и полный перенос renderer этим результатом не заявлены.

## Измеренная подготовка и original execution

Отдельный research-профиль `protected-constructor` разрешён для этого consumer:
6M инструкций / 24s на вызов, child 30s, один worker. Старые micro/file/character
сохранены; семь profile/arena tests прошли. Профиль выбран до fresh ReaderFixture.
Использованы прежние 64KiB heap, отдельное размещение renderer `35000000` на
64KiB и actual request 62,312B. Pristine PE, allocator/stream seams и `_code`
guard сохранены; VM, game functions и defaults не подменялись. GPU не запускался.

Цепочка `4C5AB0 → 4C5A10 → 4AE370 → 4563F0 → 13B85C0` прошла original
record611 lookup и XOR decryption. Естественный byte exit случился точно на
4,683,530 — как предсказывал прежний finite-loop proof. Диапазон `13B85C8`,
1,190B, SHA256 `53B8A07B289263140620B288A714CE65AE564DB9EB313918550C865791170E9C`
сохранён вместе с предшествующим FS-prefix64 в отдельном constructor window.
Report и все allocations/write masks сохранены до уничтожения гостя; original
renderer destructor не вызывался.

## Actual producer и результат после return

| Original instruction / call | Действие |
|---|---|
| `13B890E…13B8918 → 4A9460` | existing DXMaterial factory, 188B |
| `13B891D` | записывает результат в renderer `C9C0` |
| `13B8923…13B892D → 45F610` | existing pass factory, 56B |
| `13B8948 → 423960` | append pass по текущему material count0 |
| `13B8956`, `13B8981 → 460E50` | два StdLayer, каждый 20B + MaterialTexture104B |
| `13B896B`, `13B89A0 → 45F5E0` | append layers по текущим count0/1 |
| `13B8991` | второй MaterialTexture raw state1 получает0 |

Snapshot сделан после обычного полного factory return. Все ссылки, extent и
write masks проверены; первый store `C9C0` сам по себе не считается готовым graph.

| Semantic state | Actual PC |
|---|---|
| Material class / pass count | `797B39EC` / 1 |
| Render states0…10 | `[0,0,1,2,1,1,3,0,4,1,6]` |
| Diffuse / specular | `(1,1,1,1)` |
| Ambient / emissive | `(0,0,0,0)`; это не MaterialColorController saved-alpha1 |
| Power initialized | false: `+B8` не записан; allocatorCC не является default value |
| Pass class / blend / layers | `3A8905A5` / 0 / 2 |
| Layer0 states0…8 | `[0,3,1,0,0,FF000000,2,0,0]` |
| Layer1 states0…8 | `[0,0,1,0,0,FF000000,2,0,0]` |
| Оба holder | identity UV, static-UV false, NULL texture/animation/UV edges |

Earlier [PS2 static producer](tool-renderer-fallback-material-boundary-2026-09-10.md)
подтверждает ту же topology, первые девять states и unwritten power независимо.
PS2 имеет другой concrete material/layout; его данные не использовались как PC
seeds и не подменяли PC evidence.

## Общая реализация и границы

`spRenderer::CreatePCDefaultMaterialForAnalysis()` возвращает owned
`std::unique_ptr<spDXMaterial>` и воспроизводит только этот PC producer, вызывая
existing material/pass/layer constructors. Дополнительных color/power setters
нет: они уже соответствуют actual PC defaults. Метод не создаёт renderer,
Direct3D device или Viewer-specific material factory.

`spRendererSceneTests` проверяет snapshot, независимость produced graphs и
рекурсивное host ownership через weak leaf edge. В `spRendererSubmitTests`
новый case `actual-default` передаёт producer в existing whole unlit Submit:
проверяются fallback states unused stages1/7 и сохранение unknown power.
Исторические `--case` inputs для старых native comparisons сохранены.
Native validation root прошла: RendererScene 574/574 (0.76s), RendererSubmit
201/201 (1.19s), FullLoader 213/213 (0.72s); CTest 3/3 PASS, всего 4.19s.
Сборка сохраняет ранее существующий warning C4756; утверждения «0 warnings» нет.
Build-Native/CMake получили адресные RendererScene/RendererSubmit suites.
[Build log](../../local-data/results/tools-core-cycle-20260910-0730/material-preview/fallback-material/default-material-native-build.log),
[CTest log](../../local-data/results/tools-core-cycle-20260910-0730/material-preview/fallback-material/default-material-ctest.log)
и immutable DLL `default-material-SparkplugViewerNative.dll` закреплены в manifest.
DLL SHA256: `0683959547EE5E77AD0B186B055D81F8EDA39BC6239EF1F067C5C48844DC6A10`.

[Manifest](../../research/tools-core-pc-renderer-default-material-2026-09-10.json)
содержит source/evidence hashes.
[Original report](../../local-data/results/tools-core-cycle-20260910-0730/material-preview/fallback-material/renderer-constructor-protected6m-run1.json)
и [адресный analysis](../../local-data/results/tools-core-cycle-20260910-0730/material-preview/fallback-material/renderer-constructor-protected6m-analysis.json)
ссылаются на локальные allocations, coverage и decoded range. Игровые bytes в Git
не добавляются. Полный renderer state, дальнейшие startup writes, renderer
teardown и новый backend multipass остаются отдельными consumers/проверками.
