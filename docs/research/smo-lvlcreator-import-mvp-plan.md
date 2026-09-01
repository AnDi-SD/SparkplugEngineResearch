# MVP-план SmoLVLcreator и импорта моделей

Статус: утверждённая сокращённая граница исследования с 29 августа 2026 года.
Полный runtime-план сохраняется как post-MVP backlog, но не блокирует первый
практический выпуск редактора и импортера.

Текущий прогресс: все 7 из 7 рабочих шагов пройдены 29 августа 2026 года.
Отчёты, SHA-256, native-матрицы и
render evidence находятся в
[`smo-native-gate0-results.md`](smo-native-gate0-results.md) и
[`smo-native-gate2-results.md`](smo-native-gate2-results.md), а rigid/material
результаты — в [`smo-native-gate35-results.md`](smo-native-gate35-results.md).
Collision-контракт, Group 2, `wxFaceData` и gameplay evidence зафиксированы в
[`smo-native-gate6-results.md`](smo-native-gate6-results.md).
Skinned-палитры, native one-pass reference order, Bloom/Flora и SAN-деформация
зафиксированы в [`smo-native-gate4-results.md`](smo-native-gate4-results.md).
Container invariants, zero-edit SHA, atomic install, backups и writer native-матрица
зафиксированы в [`smo-native-gate1-results.md`](smo-native-gate1-results.md).
Итоговая end-to-end/stress-матрица, негативные memory/timeout/cancellation tests
и gameplay evidence зафиксированы в
[`smo-native-gate7-results.md`](smo-native-gate7-results.md).

## Критерий обязательности

В MVP входит только вопрос, который влияет хотя бы на одно из действий:

- открыть и без потерь сохранить существующий PC SMO;
- переместить, повернуть или масштабировать объект уровня;
- добавить, заменить, дублировать или удалить модель;
- импортировать rigid-модель в уровень;
- импортировать skinned-модель через SmoImporter;
- сохранить необходимые geometry, material, texture, skin и collision связи;
- собрать файл, который принимает и корректно показывает оригинальная PC-игра.

Неизвестное поле не блокирует MVP, если оно не редактируется, сохраняется
байт-в-байт и не участвует в пересчитываемом relationship graph. Поле, для
которого это невозможно доказать, остаётся read-only и блокирует только
соответствующую операцию, а не весь редактор.

## Gate 0 — воспроизводимый native baseline — пройден

1. Зафиксировать contextual route и scene-ready checkpoint хотя бы для одного
   небольшого уровня, основного Alfea regression-уровня и одного персонажа.
2. Для каждого release-кандидата выполнять `pristine -> mutation -> pristine`.
3. Различать loader acceptance, scene-ready, первый render и gameplay interaction.
4. Сохранять executable/resource hashes, JSONL, screenshot и результат.

Условие готовности: неизменённый контроль стабильно загружается, а редакторский
файл доходит не только до FFPS loader, но и до наблюдаемой готовой сцены.

## Gate 1 — container и writer safety — пройден

Обязательные инварианты:

- zero-edit build воспроизводит исходный SHA-256;
- известные и неизвестные неизменённые payload сохраняются точно;
- после add/remove/replace пересчитаны catalog offsets, object sizes, enclosing
  field sizes и inline size prefixes;
- все ID/reference/inline relationships разрешаются;
- удалённые ветви недостижимы, общие ресурсы не дублируются и не теряются;
- project save/reopen и повторный build детерминированы;
- strict parser, object-reference audit и native load обязательны;
- запись атомарна, исходник не меняется, существующий output получает backup.

Не требуется заранее разрешать произвольное ручное редактирование object ID или
relationship. Требуется доказать только операции, которые реально генерируют
SmoLVLcreator и SmoImporter.

## Gate 2 — transforms и placements — пройден

На реальных владельцах visual transform — `spNode`, `spRenderNode` и
`spStaticRenderObject` — проверить (существующий collision object проверяется
вместе с полным production collision path в Gate 6; сам
`spModel` position/rotation/scale не сериализует и получает размещение от
родительского node либо `spStaticRenderObject`):

1. Position по каждой оси;
2. Rotation вокруг каждой оси и порядок quaternion;
3. uniform и nonuniform Scale;
4. WORLD/LOCAL composition;
5. парную запись Transform/InvTransform по convention движка;
6. duplicate/shared placement без копирования физического ресурса;
7. add/remove placement и повторное открытие результата.

Условие готовности: viewport, сохранённый SMO и игра показывают одинаковый
transform без mirror, double-transform и изменения соседних instances.

## Gate 3 — rigid model import — пройден

Проверить production-пути OBJ, GLB и FBX на небольшой и составной модели:

- оси, единицы, handedness, winding и culling;
- positions, normals, UV0; UV1 и vertex color/alpha, когда они присутствуют;
- triangle list, UInt16 index limits и детерминированное разбиение больших meshes;
- несколько mesh/material частей;
- отдельные и общие textures без повторного встраивания;
- opaque и alpha-поверхности, которые реально создаёт importer;
- add, complete replacement, duplicate instance, delete, Undo/Redo;
- build/reopen/re-import и native scene-ready.

Полный реверс всех material states не требуется. Importer использует только
подтверждённые production presets; неподдерживаемое состояние должно быть
сохранено от target либо явно отклонено.

## Gate 4 — skinned model import — пройден

Это gate SmoImporter, но не разрешение skinned level assets в SmoLVLcreator.

Обязательно подтвердить:

- регистрозависимые exact bone names;
- hierarchy и target bind pose;
- нормализацию и допустимое число influences;
- 16-slot PC palettes и разбиение triangles без потери weights;
- inverse-bind matrices и отсутствие double-transform;
- target-rig fitting либо безопасный отказ при неоднозначности;
- сохранение rigid attachments;
- проигрывание минимум одной штатной SAN на Bloom и одном отличающемся target;
- отсутствие missing/ambiguous binding в production output.

Поведение игры при намеренно missing или duplicate node name не блокирует MVP:
production validator обязан такие результаты отклонять. Полные ANM states,
`AdvBloom.anm` и service/DCC tracks остаются post-MVP.

## Gate 5 — минимальные materials и textures — пройден

Нужно доказать только состояния, которые writer создаёт или изменяет:

1. opaque textured;
2. texture alpha / vertex alpha;
3. masked либо blended surface, если такой preset доступен в UI;
4. additive только если importer действительно его генерирует;
5. texture ownership и alpha preservation;
6. безопасные размеры, memory budget и отказ до записи при превышении лимита;
7. отсутствие неожиданной смены render order у соседних target-материалов.

Все прочие `FinalBlendOp`, state slots, UV/material controllers и animated
textures сохраняются от исходника и остаются read-only.

## Gate 6 — collision, необходимая редактору — пройден

Для существующей и сгенерированной collision branch проверить:

- add, transform, link/unlink, regenerate и delete;
- корректный registry owner и полное удаление inline branch;
- выбранный production default collision Group;
- движение персонажа и camera collision;
- совпадение visual placement и generated hull после build;
- сохранение triangle order и `wxFaceData` byte-for-byte.

Редактирование `wxFaceData.flags`, `surfaceID`, navigation, portals и route cost
в MVP не входит.

## Gate 7 — end-to-end release matrix — пройден

Минимальная матрица:

1. маленький уровень для быстрой диагностики;
2. `Alfea02_old.smo` как основной реальный regression;
3. один более крупный многомодельный уровень;
4. Bloom и второй target для skinned importer.

На уровнях выполнить одной серией:

- transforms;
- shared placement;
- внешний rigid import;
- complete replacement;
- texture replacement;
- collision add/delete;
- project archive/reopen;
- два одинаковых build;
- strict/reference audit;
- native scene-ready и короткую игровую проверку.

Отдельно сохранить stress gate для memory, timeout, cancellation и отсутствия
частично установленного output.

## PC и PS2

MVP SmoLVLcreator и production import сейчас целятся в PC. PS2-корпус продолжает
использоваться для проверки общих serializer-контрактов и хранится в research DB,
но PS2 mesh/texture writer, emulator runtime matrix и PS2 output не блокируют PC
выпуск. Если PS2 export станет пользовательской функцией, для него потребуется
отдельный release gate.

## Post-MVP backlog

До следующего этапа откладываются:

- missing-name final PRS/world fallback и duplicate final matrices;
- полный разбор ANM states, `AdvBloom.anm` и service tracks;
- исчерпывающие `FinalBlendOp`, render/texture states и controllers;
- navigation, portals, occlusion и BSP editing;
- fog, lights, particles, lens flare и sky authoring;
- GUI anchors, runtime text, code page, wrap и alignment;
- ненаблюдавшиеся optional serializer fields;
- произвольный object/relationship editor;
- полный SPT/SPL;
- PS2 DMA/VIF, skin, texture swizzle и output writer.

## Порядок выполнения от текущей точки

1. Gate 0: scene-ready native routes — пройден.
2. Gate 2: transforms/placements в игре — пройден.
3. Gate 3 и Gate 5: rigid geometry, materials и textures — пройдены.
4. Gate 6: production collision path — пройден.
5. Gate 4: skinned importer и две реальные анимационные проверки — пройден.
6. Gate 1: финальный audit всех структурных операций — пройден.
7. Gate 7: end-to-end release matrix и stress — пройден.

После прохождения этих gate исследовательский этап для первого запуска считается
завершённым. Все остальные вопросы остаются документированными, но не задерживают
разработку и выпуск.
