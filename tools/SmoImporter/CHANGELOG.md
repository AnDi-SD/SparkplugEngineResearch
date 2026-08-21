# Changelog

## 0.4.0 — 2026-08-22

- разрозненные элементы выбора embedded-текстуры, одиночного внешнего файла и
  папки `matN` заменяются единым списком текстур для импорта; панель перенесена
  сразу после выбора donor, чтобы material mapping был готов до подгонки и нарезки;
- внешний диалог поддерживает пакетный выбор изображений, а добавленные вручную
  ресурсы можно удалять из списка без повторной загрузки модели;
- material → texture сопоставление переводится на стабильный индекс исходного
  изображения вместо одного лишь имени, которое может отсутствовать или
  дублироваться;
- отсутствующие external image references остаются видимыми в списке как
  ожидаемые файлы и блокируют неточный импорт до разрешения;
- rigid и skinned writers получают отдельное изображение каждой material group;
  папка без `matN` добавляет все поддерживаемые изображения в тот же каталог, а
  строгая `matN`-папка сохраняет последовательности кадров;
- legacy single-atlas writer читает встроенный image payload напрямую, без
  временного файла и зависимости от MIME; пакетный список принимает PNG, JPEG,
  BMP и TGA;
- добавлен диагностический режим «Оставить текстуры исходного SMO»: rigid и
  skinned geometry импортируется без изменения или добавления `TextureData`;
  multi-material rigid geometry при этом явно сводится в один исходный body-slot;
- добавлены автовыбор и ручной выбор трёх режимов подготовки внешней модели;
  после отказа режима 1 Auto проверяет safety-анализ режима 2 и переключается на
  режим 3, если все активные donor weights нельзя сопоставить без догадок;
  нечитабельный GLB/FBX rig можно загрузить только как geometry с сохранением
  диагностической причины, а не передавать его некорректные weights дальше;
- режим 1 показывает тот же target bind pose, который сериализует writer;
  исправлена однократная external-space ↔ SMO reflection вместе с winding и
  проверкой geometry fingerprint, а несколько donor skins канонизируются по имени
  только при совпадающих inverse-bind matrices;
- режим 2 строго адаптирует существующие donor weights к target deform rig:
  неизвестные активные joints не угадываются, geometry переводится в target bind
  pose, совпавшие influences складываются и нормализуются до четырёх;
- для режима 3 добавлен отдельный fail-loud pipeline генерации weights:
  legacy-вызов сохраняет строгий выбор одной dominant surface, а новый явный
  `TargetRigBodySelection` позволяет детерминированно объединить геометрически
  выбранные нижнюю часть тела и торс/руки. Обе поверхности получают smooth
  side-aware weights по капсулам безопасных target bones, а отделённые части не
  удаляются и возвращаются rigid attachments с обязательным подтверждением;
- capsule distance центральной поверхности дополнен откалиброванными по исходным
  target weights анатомическими полями. Эллиптические объёмы вдоль позвоночника и
  отдельный Head-эллипсоид сдвинуты вперёд так, что их задняя стенка касается оси
  соответствующих костей. Конечное плавное затухание сохраняет прежние scores вне
  центральной области; нижняя граница корпуса не затрагивает ноги, а руки и ноги
  вне полей остаются на прежнем алгоритме;
- перед генерацией weights/attachments режим 3 получил отдельный первый этап
  размещения donor geometry. Автоподгонка по полным bounds заполняет абсолютные
  uniform scale + translation как стартовые значения, после чего пользователь
  может скорректировать их вручную; rotation этого transform пока запрещён;
- неприменённая правка размещения блокирует plan/save до **Применить**. После
  применения alignment уже входит в `FittingPreviewScene`/`PreparedScene`, поэтому
  preview и writer не накладывают его второй раз. Target skeleton, длины и IBM при
  размещении не меняются;
- редактор подгонки позы разделён на два расширяемых режима. По умолчанию открыт
  режим **Суставы**: deform-сустав выбирается в списке или щелчком в preview, а три
  ползунка задают абсолютные local Euler angles X/Y/Z от −180° до +180°;
- режим **Человек** содержит детерминированную **Автоподгонку** и симметричные
  параметры подъёма/направления рук, сгиба локтей, разведения/сгиба ног, наклона
  корпуса и движения шеи вперёд/назад. Исправлено предпочтительное направление
  локтей: двухсегментная цепочка теперь сгибается в анатомически ожидаемую сторону.
  NeckForward добавляет bind-local counterrotation `Head`, поэтому голова сохраняет
  мировое направление вперёд, не стирая отдельную ручную поправку;
- оба режима используют одну общую transient-позу. Переключение режима не сбрасывает
  значения, результат **Человека** отображается в абсолютных ползунках **Суставов**,
  а rebase high-level controls сохраняет ручные коррекции. Live draft сразу
  skin-деформирует серый target mesh и до **Применить** блокирует plan/save. Ручное
  применение больше не требует предварительного AutoFit: `SelectBody` разрешает
  body selection отдельно от optimizer-а;
- generated-skinning topology нормализуется по общему правилу без имён
  моделей: нулевые triangles и вершины, не входящие ни в одну рисуемую
  грань, не создают ложные components и не блокируют подготовку. Исходные
  vertex slots сохраняют индексы и получают inert placeholder weights; неполные
  или выходящие за границы индексы и non-finite данные остаются hard blockers;
- реальная regression Flora FBX проходит нейтральную и ручную позы: 30 meshes и
  2 758 vertex slots сохраняются, а triangle stream нормализуется из 3 707 в 3 375
  рисуемых граней; 332 degenerate triangles и 71 non-surface vertices не влияют на
  components, weights и видимую геометрию;
- target skeleton дополнительно рисуется поверх геометрии screen-space overlay:
  его не закрывает mesh, выбранный сустав подсвечивается и остаётся доступным для
  выбора кликом. Canonical bind matrices, IBM, hierarchy и длины костей не меняются;
- в режиме 3 добавлена секция **Человек → Жёсткие детали**. Отделённые связные
  компоненты выбираются в списке или точным щелчком по их треугольникам в preview;
  `Ctrl` поддерживает множественный выбор, выбранные детали подсвечиваются. **К спине**
  задаёт one-hot `Spine_03`, **К голове** — one-hot `Head`, а возврат автоматического
  назначения снимает override. Галочка скрытия основного donor body действует только
  на authoring preview и не меняет selection, weights, attachments или writer input;
- ручные component overrides сохраняются при правке позы и alignment, переключении
  режимов и текстур. Core fail-loud сверяет fingerprints target/donor, число
  компонентов и точную vertex membership; назначить можно только detached component,
  а часть сваренной smooth-поверхности этим инструментом отделить нельзя. Существующий
  сгенерированный SMO для применения новых весов и привязок нужно создать заново;
- режим 3 требует заранее upright, Y-up, unmirrored и same-facing donor. GUI
  показывает alignment, предупреждения и rigid attachments; Core всегда выставляет
  `RequiresConfirmation`. Галочка доступна только после того, как канонический
  `PreparedScene` текущей ревизии хотя бы один раз показан вместо fitting pose;
  plan/save разблокируются лишь после этого просмотра и явного подтверждения;
- для Layla OBJ/FBX геометрический selector без зависимости от имён выбирает две
  body-поверхности из 43; обе получают generated smooth weights, остальные 41
  компонент, включая восемь частей крыльев, остаются rigid attachments. Selection
  валидируется по точным target/donor/alignment/topology fingerprint и membership;
- для Layla снят geometry/palette blocker: семь material groups могут быть
  детерминированно упакованы в один 256×256 RGBA-atlas с переписанными UV и wrapped gutters,
  а точный backtracking allocator распределяет triangles по существующим
  16-костным palettes. Режим 3 адаптивно пробует пределы в 4, 3 и 2 влияния и
  уменьшает их только после доказанного capacity failure; Layla использует максимум
  три. Один RGBA-atlas теперь разрешён и для смешанного Layla import, но его
  consumers разделяются: 2 714 opaque body triangles остаются на существующих opaque
  `spSkin/material` branches target, 124 triangles `mat3`/глаз и `mat4`/рта добавляются
  двумя независимыми opaque-overlay runs, а 70 настоящих alpha triangles — тремя
  runs `mat5:34`, `mat7:34`, `mat6:2` со ссылкой на тот же atlas. PNG Alpha сам по
  себе не считается семантикой материала: явная **Непрозрачная накладка** ставит
  выбранной texture group `A = 255` до premultiplied resize, сохраняет RGB под прежним
  `A = 0` и использует канонический eye tuple `FinalBlendOp = 0`,
  `MaterialRenderStates = [0, 0, 1, 0, 1, 1, 3, 0, 4, 0, 6]`,
  `LayerTextureStates = [0, 3, 3, 0, 0, 4278190080, 2, 0, 0]`,
  `AlphaSortEnable = 0`, `Priority = 1` и vertex diffuse `0xFFFFFFFF`. Белый diffuse
  совпадает с generated retained body для OBJ без vertex colors. Нормали face overlay
  не сплющиваются и не заменяются. Для Layla этот режим
  выбирается у `mat3`/`mat4`; `mat5`/`mat6`/`mat7` остаются в **Авто**. Профиль
  donor-bound и одинаков для Analyze/preview/write; default `Auto` не изменён.
  Palette/ushort continuation
  может быть material-less только внутри того же исходного renderable; разные overlays
  больше не bin-pack'ятся в один native sort unit. Все ветви сохраняют
  текущие target weights, inverse-bind matrices и palettes;
- production-контракт настоящей skinned alpha-ветви перенесён из нативного comparator
  `Minautor.smo`:
  `FinalBlendOp = 2`,
  `MaterialRenderStates = [0, 0, 1, 0, 1, 1, 3, 0, 4, 0, 6]`,
  `LayerTextureStates = [0, 3, 3, 0, 0, 4278190080, 2, 0, 0]`,
  `AlphaSortEnable = 1`, `Priority = 1`, vertex diffuse `0xFF000000`, отдельная
  цепочка `spSkin → material → mesh` и shared texture reference. Произвольный
  `FinalBlendOp = 2` сам по себе не классифицируется как Alpha: нужны точный
  consumer state и покрытие прозрачных texels его UV. Каждый самостоятельный source
  renderable получает собственный material-bearing run; material-less допустимо
  только palette/ushort-продолжение того же renderable. Старый tuple тиары
  `bloom_princess.smo` с `MaterialRenderStates[5] = 0` и `AlphaSortEnable = 0`
  остаётся структурным примером малой локальной ветви, но не подходит крупным
  skinned-крыльям: в игровом тесте более далёкие alpha-поверхности листвы и уровня
  могут перекрывать такие крылья;
- `FinalBlendOp = 4` раннего importer относился к glow/effect-семейству, а
  `FinalBlendOp = 6` из других fixtures не является универсальной заменой. Ранний
  importer правильно сохранял material groups раздельно, но использовал неверные
  Head-only weights. Текущий split сохраняет новые generated weights. `mat6`
  остаётся отдельной двухтреугольной alpha-подвеской поверх opaque body branch,
  поэтому её прозрачное окно не переводит всё тело в прозрачный consumer;
- rigid packer сам создаёт one-bone `spSkin`: прежняя policy
  `FinalBlendOp = 6`/`RS[8] = 2` для него не подтверждена. Привязанные alpha frames
  блокируются в `Analyze` и повторно перед записью; opaque groups сохраняют target state;
- Viewer и Blender проверяют геометрию, UV, содержимое atlas и наличие Alpha, а
  native validator — корректную загрузку ресурса. Они не воспроизводят native blend,
  lighting и depth/sort: прежний единый alpha-run выглядел корректно в WPF, но в игре
  скрывал лицо и показывал украшения только под частью углов. Поворот камеры при
  фиксированном world-light Viewer также способен скрыть углозависимую ошибку.
  Визуальный паритет нового контракта не заявляется до пользовательского теста вновь
  созданного SMO в игре;
- workflow режима 3 сделан последовательным: сверху показывается одно главное
  действие или реальная причина блокировки, отдельная кнопка открывает точный
  канонический итог для подтверждения, а десятки технических предупреждений и
  rigid attachments находятся в свёрнутой ограниченной по высоте секции;
- перед автоматической нарезкой добавлен окончательный текстурированный preview. Он
  использует тот же preparation path, что writer, и показывает фактические atlas,
  переписанные UV и импортируемые изображения. В диагностическом режиме сохранения
  target-текстур остаётся явная одноцветная review-кнопка, доступная до зачёта
  текущей ревизии даже при уже видимом canonical scene. Для legacy rigid `matN`
  final preview отключён, поскольку сырой viewport не совпадает с packer/writer;
- skinned или частично skinned вход режима 3 целиком перечитывается geometry-only,
  без удаления отдельных meshes. Внешние texture overrides сохраняются. Папка
  `matN` остаётся rigid-only: bundle полностью игнорируется, его frames не
  показываются, а кнопка папки отключена в режиме 3. Вместо неё используется
  пакетное **Добавить файлы…** с однозначным material mapping;
- режим **Суставы** меняет только абсолютные local rotations временной позы;
  `FittingPreviewScene` показывает результат, а geometry для writer обратно
  запекается в canonical target bind как `PreparedScene`. Euler-представление имеет
  обычную неоднозначность и gimbal lock и не заменяет quaternion pose внутри Core;
- для режима 2 ручная подгонка опционально включает weights-only подготовку и
  uniform donor alignment. В режиме 3 разрешены только local rotations, а root
  transform отключён из-за canonical-space donor alignment;
- target service/skeleton graph во всех режимах остаётся исходным. **Человек**
  использует симметричный two-bone IK; выбор сустава кликом уже доступен, но viewport
  gizmos для прямого перетаскивания/вращения пока отсутствуют. Donor должен быть
  Y-up/same-facing, а анимации требуют ручной проверки в игре;
- режим создания weights с нуля остаётся экспериментальным: build, strict writer и
  native loader не доказывают качество weights, анимацию и итоговый blend/depth;

## 0.3.0 — 2026-08-14

- режим SMO → SMO переведён на append-only visual packing: service/skeleton graph,
  IDs и неизвестные связи target сохраняются, старые meshes остаются невидимыми
  structural anchors, а donor `spSkin`/material/texture/UV/mesh branches получают
  новые уникальные IDs;
- упаковщик переносит visual forest из нескольких render roots, отдельные rigid-
  детали, несколько texture groups и нативные texture sequences без atlas fallback;
  контрольные Faragonda → Bloom и StellaX ↔ Bloom прошли strict и native проверки;
- добавлен rigid multi-material импорт GLB, OBJ и FBX. Материалы связываются с
  PNG через metadata либо OBJ `mtllib`/`usemtl`/`map_Kd`; GUI принимает отдельную
  папку с `matN.png` и последовательностями `matN.1.png`, `matN.2.png`;
- non-POT текстуры увеличиваются до следующей степени двойки по каждой оси,
  никогда не уменьшаются и ограничены 2048 пикселями. Исходные opaque POT-пиксели
  сохраняются; привязанные frames с `A < 255` теперь fail-loud отклоняются до файла,
  поскольку alpha-контракт generated one-bone `spSkin` не подтверждён;
- исправлено зеркальное поле texture header `+0x38`: оно хранит `height << 8`.
  Формула подтверждена на 2 348 pristine textures и устранила нативный crash
  прямоугольных атласов 128×64;
- skinned GLB/FBX transfer разделён на режимы `RetargetToGameBindPose` и
  `PreservePreparedGeometry`. Production-режим выполняет ровно один bind-pose
  rebase, но записывает только target bone IDs, target graph и canonical target
  inverse-bind matrices — donor nodes и animation logic не переносятся;
- skinned texture replacement ограничен сопоставленным body-atlas. Материалы с
  одним source image объединяются, а глаза, эффекты и непарные texture objects
  остаются побайтно исходными;
- подготовленная Текна перенесена в чистый `Characters/Tecna/Tecna.smo`: 17 active
  joints совпали точно, 382 triangles распределены по семи игровым palettes,
  `tecna_e` сохранена. Результат прошёл strict parser, быстрый native smoke test и
  две контекстные загрузки на Cloud01_02 в оконном режиме;
- исправлен off-by-one texture parser/writer: для `0x32E3`/`0x43E3` marker `00`
  находится на `+0x3C`, а BGRA payload начинается с `+0x3D`;
- после сохранения GUI автоматически проверяет новый SMO нативным загрузчиком
  игры в быстром изолированном оконном режиме. Пользователю доступны только путь
  к `WinxClub.exe` и краткий результат «подходит / не подходит / не определено»;
- GUI не разрешает перезаписать target или donor. Skinned-режим также блокирует
  AutoFit и произвольный transform, которые могли бы повторно применить позу;
- FBX использует общий GLB pipeline через Blender, с расширенным автопоиском
  `blender.exe` и исключением helper meshes без Armature.
- GLB reader распознаёт безопасное palette padding старого SmoExporter: повтор
  одного glTF joint node с побайтно одинаковой inverse-bind matrix схлопывается с
  remap `JOINTS_0`; одинаковые имена разных nodes и разные matrices по-прежнему
  блокируются как неоднозначные.

## 0.2.0 — 2026-08-13

- добавлен экспериментальный skinned GLB → SMO writer: импорт `JOINTS_0`,
  `WEIGHTS_0`, inverse bind matrices, vertex colors и material primitives;
- добавлен bind-pose rebase внешней модели на сохранённый skeleton target,
  нормализация weights и автоматическая нарезка по 16-костным palettes;
- GUI распознаёт skinned GLB, показывает дерево сопоставления костей и блокирует
  запись при неизвестных active joints;
- добавлен skinned FBX → GLB → SMO через Blender с общим bone/palette/RGB pipeline;
- добавлен консервативный режим SMO → SMO: target object graph, IDs, skeleton,
  service objects и неизвестные ссылки сохраняются, меняются visual leaf payload
  и reference-only palettes;
- различающиеся donor bones сворачиваются в ближайшие совместимые target bones,
  а weighted hierarchy сравнивается с пропуском helper/control nodes;
- подтверждённые vertex layouts конвертируются field-wise, включая
  `0x093E → 0x097E`, генерацию normals и объединение weights;
- writer пересчитывает каталог, object sizes и offsets, повторно проверяет
  результат strict parser и не разрешает перезаписать target или donor;
- пользовательский пакет переведён на единый framework-dependent single-file
  формат с загрузчиком .NET 8 Desktop Runtime.

## 0.1.1 — 2026-08-12

- GUI принимает путь исходного SMO в командной строке для запуска из SmoViewer;
- в SmoViewer добавлена отдельная кнопка «Импорт…» рядом с запуском экспортера.

## 0.1.0 — 2026-08-10

Первый экспериментальный релиз SmoImporter.

- импорт целой OBJ/GLB-сцены и новая triangle-list topology;
- single-slot rigid mesh replacement с ручным выбором подтверждённой bone palette;
- автоматическая подгонка размера, центра, вращения и смещения;
- сохранение остальных render slots через проверенные degenerate triangles;
- embedded GLB base-color и внешний PNG/JPEG;
- подтверждённая игрой fixed-size замена только RGB исходного ABGR atlas;
- исходные Alpha, headers, offsets, object graph и размер texture block сохраняются;
- запись всегда выполняется в новый SMO с повторной проверкой strict parser.

Изменение разрешения atlas, texture repack, скелетная деформация и импорт анимаций
в этой версии не поддерживались.
