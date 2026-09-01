# Gate 1: container и writer safety

Дата проверки: 29 августа 2026 года.

Статус: **пройден**. Основной project writer SmoLVLcreator и все файловые
writer-пути SmoImporter теперь проверяют временный SMO до установки, не позволяют
писать поверх входного файла, атомарно заменяют существующий результат и сохраняют
его в timestamped `.bak`.

## Закрытые инварианты

| Инвариант | Доказательство |
|---|---|
| Zero-edit SHA-256 | `Alfea02_old.smo` после import/archive/reopen/build: те же 7 106 313 байт и SHA-256 `1316A81D27254E1B20627433CC8E01327041D560BBEFACB949ADF38A336B79DF`. |
| Известные и неизвестные неизменённые bytes | `SmoProjectSerializer.Verify` сравнивает каждый промежуток исходного `data.bin` до, между и после запланированных изменений; zero-edit сравнивает весь контейнер. |
| Catalog, object, field и inline sizes | Copy-on-write layout planner и 1 864 Core assertions проверяют add, materialize, move, redirect, replace и remove с повторным строгим разбором. |
| ID/reference/inline graph | Проверены reference placement, resource redirect/relocation, registry links, added forests, точный field occurrence и native one-pass skin references. |
| Reachability и shared resources | Branch removal сохраняет внешне используемые descendants; physical owner promotion и повторные placements не копируют mesh/material/texture payload. |
| Archive/reopen и determinism | Реальный Alfea project повторно собран с тем же SHA; ранее зафиксированный 1 000-operation stress также дал одинаковые build/re-import SHA. |
| Strict и native | Все контрольные outputs проходят строгие writer-verifiers; native-матрица ниже прошла 5/5 scene-ready. |
| Atomic install и backup | Новая regression — 11/11 assertions; verifier failure оставляет старый output неизменным, input overwrite отклоняется, временные файлы удаляются. Каждый реальный writer повторно записан поверх результата, backup совпал с предыдущим SHA. |

Произвольные неизвестные поля не интерпретируются как references. Они остаются
opaque и byte-preserved; writer изменяет только подтверждённые direct fields и
отношения, которые сам создаёт.

## Исправленный риск SmoImporter

Аудит обнаружил, что project writer SmoLVLcreator уже выполнял безопасную
установку, но отдельные API SmoImporter использовали обычный overwrite. Legacy
whole-model режим проверял уже установленный файл, поэтому ошибка строгого
verifier могла оставить повреждённый output.

Общий `SmoVerifiedOutputInstaller` теперь используется всеми файловыми writer’ами:

- topology-safe single mesh;
- legacy whole model;
- rigid multi-material;
- SMO→SMO visual transplant;
- skinned GLB/FBX transplant.

Порядок операции один: запрет совпадения input/output → same-directory temporary
file → flush-to-disk → writer-specific strict verification → `File.Replace` с
backup либо atomic move для нового файла → контроль SHA установленного результата.

## Реальные outputs

| Режим | Байты | SHA-256 |
|---|---:|---|
| Alfea zero-edit project | 7 106 313 | `1316A81D27254E1B20627433CC8E01327041D560BBEFACB949ADF38A336B79DF` |
| Bloom + Tecna legacy whole-model | 132 567 | `D5E1CC02A82AC067B0BB7144601718FEF74FB1434DE92D46A8DEF57C0200A6BC` |
| Bloom + Layla rigid multi-material | 2 642 346 | `96F10491BDD43848B8D88DB69C260AAE636D5D2A02D22EF6A4688434A94DBE55` |
| Bloom + Faragonda SMO→SMO | 1 001 798 | `DB0FB0642BDAB20F0062DAF369B73B2C5BFEB7895DF41C0355CC8006FE49A1FF` |
| Bloom + Miku skinned GLB | 5 847 737 | `6734C5FE801657CB12C5AE86EAE97261D0B304B075603BF2B7D40CAF23564B2D` |

Повторная генерация каждого importer output дала тот же SHA. Рядом с каждым
существующим output появился `.bak` с тем же предыдущим SHA; исходные SMO, GLB,
OBJ и textures не менялись.

## Native matrix

Manifest:
[`mvp-gate1-writer.json`](../../tools/SmoViewer/SmoNativeValidator.Cli/manifests/mvp-gate1-writer.json).

Run:
`local-data/validation-results/mvp-gate1-writer-native-20260829/run-20260829-180201-236`.

| Кейс | Scene-ready | Время | Последний checkpoint |
|---|---|---:|---|
| Alfea zero-edit | passed | 35,001 s | `FFPS03`, non-null resource + level 28 |
| Legacy whole-model | passed | 26,940 s | `CP08` |
| Rigid multi-material | passed | 25,315 s | `CP08` |
| SMO→SMO | passed | 25,243 s | `CP08` |
| Skinned GLB | passed | 25,723 s | `CP08` |

Итого: 5/5, `crash=none/none`. Все запуски использовали принадлежащие validator’у
изолированные workspace; pristine Media и executable не изменялись.

## Автоматические проверки

- `SmoLVLcreator.CoreTests` на реальном `Alfea02_old.smo`: 1 864 assertions;
- `SmoImporter.FormatTests --atomic-output-regression`: 11 assertions;
- legacy whole-model: 29 assertions;
- topology-safe single mesh: 17 assertions;
- rigid multi-material, SMO→SMO и skinned production smoke: passed;
- build `SmoImporter.FormatTests`: 0 warnings, 0 errors.

Большая release/stress-серия остаётся отдельным Gate 7: Gate 1 доказывает
корректность и безопасную установку каждой структурной операции, а Gate 7 — их
длительную совместную работу в одном проекте.
