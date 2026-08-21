# Привязка импортируемой модели к скелету

Статус: `SmoImporter 0.4.0`. Реализованы append-only путь **SMO → SMO**,
подготовка внешней модели с правильным игровым скелетом, строгая адаптация
существующих donor weights и консервативный экспериментальный режим создания
weights с нуля. Его GUI показывает анализ и rigid attachments, а plan/save требуют
явного подтверждения. Для режимов 2 и 3 workflow подгонки target rig использует
единый редактор позы с переключаемыми режимами **Суставы** и **Человек**: первый
даёт абсолютные local rotations, второй — Auto и симметричные high-level controls.

> После нативного исследования Faragonda активный путь больше не упаковывает donor
> textures в существующие target slots и не заменяет render graph целиком. Он
> сохраняет service/skeleton graph target и добавляет только полные visual branches
> донора с новыми IDs. Контрольный Faragonda → Bloom прошёл нативную контекстную
> загрузку в настоящем `bloom_jeans.smo` slot.

## Быстрый путь: готовый SMO-донор

Этот режим предназначен для замены игрового ресурса другой моделью, которая уже
находится в SMO и использует тот же скелет. Importer:

1. строго разбирает целевой SMO и SMO-донор;
2. требует одинаковый платформенный профиль и serializer;
3. сравнивает имена костей из `spSkin` со всем node graph второй модели,
   логическую иерархию и bind matrices;
4. сохраняет все target objects, их IDs, порядок, parent topology и неизвестные
   связи без удаления ветвей; старые target meshes делает невидимыми degenerate
   anchors;
5. добавляет donor `spSkin`/material/texture/UV/mesh branches с новыми уникальными
   IDs, не копируя donor nodes и service graph;
6. перестраивает donor palettes как reference-only: node IDs берутся у target по
   именам, inverse-bind matrices остаются от донора;
7. переносит donor mesh и texture payload без atlas fallback или потери Alpha;
8. пересчитывает logical offsets, serialized sizes, вложенные размеры, `FileSize`
   и `DataSize`;
9. повторно открывает результат strict parser и проверяет identity target objects,
   degenerate legacy meshes, donor geometry/UV/palettes/textures и все ссылки.

Число donor texture groups больше не ограничено числом target slots: каждая группа
переносится собственной visual branch. Неизвестные target service references при
этом не переписываются.

Дополнительные donor bones не могут быть добавлены без изменения target graph.
Их weights сворачиваются в ближайшие совместимые target bones; дерево заранее
показывает это как потерю отдельной локальной анимации.

Разница bone sets обрабатывается явно и показывается одним деревом, которое
строится тем же planner, что использует writer:

- совпавшие по имени weighted bones сохраняют обычную skinning-привязку;
- дополнительная donor bone сначала сворачивается в ближайшего общего weighted
  предка; если сериализованный graph не выражает такую связь, выбирается ближайшая
  shared bone по donor bind-space position;
- несколько weights, попавших в одну shared bone, дают ту же суммарную матричную
  деформацию; дополнительная локальная анимация при этом намеренно теряется;
- target weighted bones, отсутствующие у донора, остаются в node graph без новых
  vertex influences и выводятся как риск неправильного отображения;
- если безопасную fallback bone определить нельзя, сохранение блокируется.

Непосредственный родитель не считается deform-родителем автоматически. Planner
поднимается через невзвешенные helper/control nodes до ближайшего weighted bone и
сравнивает именно его. Совпавшая weighted hierarchy разрешается, а различающиеся
helper paths показываются отдельной веткой дерева. Если ближайший weighted parent
разный, операция по-прежнему блокируется.

Разный vertex layout также не требует raw-copy. Из подтверждённых layout полей
явно сериализуются position, normal, UV0/UV1, diffuse color, weights и indices.
При переносе `0x093E → 0x097E` отсутствующие normals строятся как нормализованная
сумма face normals. После bone remap одинаковые target influences объединяются,
оставшиеся четыре weights нормализуются.

Подгонка, выбор одной rigid bone, замена atlas и автоматическая нарезка в GUI для
SMO-донора отключаются. Они не только не нужны, но и разрушили бы уже готовое
разбиение модели по 16-костным hardware palettes.

SMO не содержит подтверждённой общей checksum. Контроль целостности составляют
`FileSize`, `DataStart + DataSize`, каталог offsets/sizes, вложенные размеры
объектов и ссылки внутри object graph. Visual transplant меняет длины mesh/texture
leaf objects, поэтому writer обязан пересчитать все затронутые поля. SHA-256
рассчитывается как внешний отчёт и не записывается в SMO.

Различие bind pose само по себе не блокирует SMO-донор: target node graph остаётся,
а donor inverse bind matrices записываются в перестроенные reference-only target
palettes для сопоставленных костей. Такой результат всё равно требует игровой
проверки. Это не тот же контракт, что у внешнего GLB: сочетание donor inverse-bind
с target animation graph переносит donor bind-логику в runtime-деформацию и при
сильном несовпадении rest pose может визуально «разорвать» модель. До реализации
mesh rebase для SMO → SMO зелёный native loader test подтверждает структуру, но не
корректность анимации.

## Режим 1: подготовленная внешняя модель

Для подготовленного GLB реализован первый writer. Он требует `JOINTS_0`,
`WEIGHTS_0`, inverse bind matrices и уникальные имена joints. Pipeline:

1. сопоставляет активные joints с deform-костями target по точным именам;
2. показывает точные пары, подтверждённые служебные remap и неиспользуемые кости
   в том же дереве GUI, которым пользуется writer;
3. нормализует до четырёх влияний и переводит каждую вершину из donor bind pose в
   target bind pose формулой `donorInverseBind × targetBindWorld`;
4. записывает palettes только с target node IDs и canonical target inverse-bind
   matrices; donor nodes, rest-pose и animation logic не сериализуются;
5. группирует primitives с одним source image в общую body-atlas группу и помещает
   её только в совместимый существующий texture group;
6. делит triangles по существующим `spSkin`, не превышая 16 костей в palette,
   локально перенумеровывает indices и дублирует vertices на границах chunks;
7. в обычном single-texture пути заменяет только RGB сопоставленного fixed-size
   BGRA texture leaf, сохраняя target Alpha; непарные eyes/effects обязаны остаться
   побайтно исходными. Если material groups donor не помещаются в target, отдельный
   multi-material путь собирает их в fixed-size RGBA-atlas и переписывает UV с wrapped
   gutters. Opaque triangles остаются в существующих target consumers, а фактически
   прозрачные triangles получают добавленные branches с общей texture reference и
   описанным ниже material-контрактом. Для
   `0x32E3`/`0x43E3` BGRA начинается с `+0x3D`;
8. сохраняет весь target object graph и проверяет hierarchy/TRS, canonical palette
   matrices, bind-frame identity, geometry, skin indices и unpaired textures.

Сцена может содержать несколько donor skins. Их joints канонизируются по точному
имени; одинаковое имя с различными inverse-bind matrices считается неоднозначным и
блокирует импорт. Исправленная граница координат сохраняет preview во внешнем
right-handed пространстве, а отражение Z обратно в SMO и смена winding происходят
только в writer. Geometry fingerprint учитывает это отражение и проверяет, что
positions/UV/weights/topology не изменились скрытым дополнительным преобразованием.

Вложенная target-ветка с palette, целиком указывающей на одну кость, считается
жёстким runtime-контрактом. Например, `bloom_eyes` находится под `Head`, поэтому
её palette нельзя переписать костями рук/ног даже при формально допустимом лимите
16. Несовместимый дополнительный material primitive можно лишь объединить с
основным body group с потерей отдельных material flags либо явно исключить.

Произвольное ближайшее соответствие по позиции запрещено. В первой контрольной
модели разрешены только три изученные служебные пары:
`C-lowerRoot → Pelvis`, `C-upperRoot → Spine_01` и `neutral_bone → Pelvis`.
Любое другое несовпавшее активное имя блокирует запись; будущий ручной редактор
соответствий должен явно хранить выбор пользователя.

`RetargetToGameBindPose` включён в GUI по умолчанию и выполняет ровно один rebase.
`PreservePreparedGeometry` оставляет positions/UV/weights неизменными и разрешён
только с identity transform для модели, уже подготовленной точно в игровой bind
pose. AutoFit и произвольный transform в skinned-режиме GUI отключены. Предпросмотр
вызывает тот же geometry-preparation path, что writer, поэтому при включённом rebase
сразу показывает target bind pose. Это статический bind preview, а не проигрывание
SAN; окончательная оценка деформации всё равно требует проверки анимаций в игре.

FBX проходит этот же путь через headless Blender-конвертацию в временный GLB.
Импортируются только skinned meshes с Armature modifier и связанные armatures;
несвязанные helper meshes намеренно исключаются. После этого применяются все те
же проверки joints, bind pose, palettes и безопасной RGB-текстуры.

## Режим 2: адаптировать существующие donor weights

Этот режим нужен, когда skinning у модели полезен, но её skeleton нельзя напрямую
считать игровым. Он не переносит donor nodes и не пытается поставить target joints
в donor rest pose. Вместо этого:

1. требует skin weights у каждого импортируемого mesh и проверяет topology,
   finite attributes, joint indices и положительную сумму weights;
2. строит canonical target skeleton только из joints, подтверждённых target
   palettes, сохраняя их точные имена и external-space inverse-bind matrices;
3. сопоставляет каждую активную donor joint точным именем, безопасно
   нормализованным именем или заранее проверенным humanoid-алиасом;
4. блокирует неизвестную active joint: spatial nearest-bone fallback намеренно
   отсутствует;
5. для каждого влияния применяет `donorInverseBind × targetBindWorld`, смешивает
   position и normal исходными weights и тем самым получает target bind pose;
6. складывает influences, которые попали в одну target joint, оставляет максимум
   четыре и заново нормализует их;
7. передаёт подготовленную сцену обычному `PreservePreparedGeometry` writer-у и
   без ручной fitting pose показывает тот же результат в preview до сохранения.

Опциональный ручной вариант режима 2 использует donor skeleton только как источник
имен и weights. Donor inverse binds не задают geometry: пользовательский uniform
alignment помещает исходные donor positions вокруг временной target fitting pose,
после чего локальные вращения target bones можно уточнить численно. Preview получает
`FittingPreviewScene`, а skin bake возвращает positions/normals в canonical target
bind для `PreparedScene`. Target names, hierarchy, bind matrices и IBM не меняются.

Неактивные donor joints не требуют соответствия. Target joints без новых weights
остаются в canonical skeleton и выводятся как предупреждение. Смешанная сцена из
skinned и unskinned meshes не исправляется удалением частей: подготовка блокируется,
пока пользователь не выберет другой явный путь.

## Режим 3: создать weights с нуля

Здесь целевой SMO — эталон bind geometry и скелета. Core принимает полностью
unskinned `ImportedScene` и действует консервативно:

До генерации weights и назначения attachments выполняется отдельный этап размещения
donor geometry. Его состояние — абсолютный `ReplacementTransform` с положительным
uniform scale, нулевым rotation и конечным XYZ translation. Явное значение полностью
заменяет automatic alignment, а не компонуется с ним. GUI использует height/center fit
по полным bounds только как старт и позволяет исправить scale/translation вручную;
крылья, удалённые аксессуары и другие выбросы могут сместить эту начальную оценку.
Core fail-loud отклоняет rotation, non-positive/non-finite scale, non-finite translation
и необратимую матрицу.

1. получает target meshes и skeleton через минимальный `SmoSceneBuilder` decode;
   любое предупреждение или неполная skinned geometry target блокирует fit;
2. считает связность только по non-degenerate triangles. Дублированные швы между
   primitives объединяются лишь при двух и более точно совпавших positions, чтобы
   единичное касание одежды не склеило её с телом. Служебные vertex slots, не
   входящие ни в одну рисуемую грань, не становятся ложными components; нулевые грани
   исключаются из подготовленного triangle stream, а нерисуемые slots с сохранёнными
   индексами получают safe placeholder weights. Неполная тройка индексов, выход за границы
   или non-finite positions/площадь остаются fail-loud ошибками;
3. у target выбирает однозначную основную skinned surface, а у donor поддерживает
   два контракта: legacy-вызов по-прежнему требует одну dominant surface; явный
   `TargetRigBodySelection` может содержать цельное тело либо пару геометрически
   найденных поверхностей «ноги/низ» + «торс/руки». Имена meshes и materials в
   классификации не участвуют;
4. использует явный абсолютный scale + translation либо, если вызывающий код его не
   передал, вычисляет conservative automatic alignment по robust bounds выбранной
   поверхности. Поворот и отражение не угадываются: donor обязан уже быть upright,
   Y-up, unmirrored и same-facing с target;
5. строит сегменты/капсулы из фактического `TargetRigDefinition`, исключает известные
   service/control names и дополняет центральную цепочку target-weight-calibrated
   анатомическими объёмами; smooth body weights для всех выбранных компонентов
   запрещают смешивать левую вершину с правой костью и наоборот;
6. оставляет четыре ближайших уникальных target influences и нормализует их;
7. каждый отделённый donor component сохраняет целиком и rigid-привязывает к одной
   ближайшей безопасной deform joint. Для него возвращаются component/mesh IDs,
   target bone, расстояние, центр и отдельное предупреждение;
8. возвращает positions в external target bind space и skeleton с точными target
   names/external inverse binds, готовый для `PreservePreparedGeometry`.

### Target-weight-calibrated объёмы корпуса и головы

Капсула хорошо описывает конечность, но её centerline недостаточно для объёмной груди
и головы: длинная капсула бедра или бицепса может оказаться ближе к передней поверхности,
чем тонкая линия позвоночника. Поэтому `GeneratedSkinningPreparer` извлекает из
исходного target SMO вершины с подтверждённым весом точных deform joints цепочки
`Spine_01 → Spine_02 → Spine_03 → Neck → Head` и использует их для калибровки
поперечных размеров и направления вперёд.

Для сегментов позвоночника строятся конечные эллиптические объёмы. Центр каждого
поперечного эллипса сдвинут вперёд на forward-полуось: его задняя стенка точно касается
posed-линии позвоночника. Для `Head` строится отдельный трёхосный эллипсоид с тем же
условием касания задней поверхности оси головы. Внутри объёма центральная кость
конкурирует как объём, а не как centerline; соседние нецентральные капсулы получают
штраф только в конечном torso field.

Между ядром и внешней границей используется конечное smoothstep-затухание. При нулевой
альфе вычисление явно возвращается к прежнему capsule score, поэтому вершины снаружи
поля не получают даже скрытого дополнительного преобразования весов. Поле корпуса
имеет жёсткую нижнюю границу на `Spine_01`, верх груди заканчивается у `Neck`, а Head
обрабатывается отдельным полем. Тем самым алгоритм исправляет центральные грудь/голову,
не меняя прежнее поведение рук и ног вне этих объёмов. Если точных костей или
достаточного набора target-weight samples нет, соответствующее поле пропускается с
диагностикой, а capsule path остаётся рабочим.

### Ручные rigid-привязки отделённых компонентов

В GUI режима 3 секция **Человек → Жёсткие детали** показывает `Attachments` как
точные связные компоненты исходной donor topology. Компонент выбирается в списке или
hit test щелчком по его треугольнику в preview; `Ctrl` переключает отдельные элементы,
`Shift` выбирает диапазон в списке, а выбранная геометрия получает отдельную подсветку.
Можно также выбрать остальные detached-компоненты того же mesh.

Галочка **Скрыть основное тело модели-донора** фильтрует только треугольники authoring
preview по vertex membership сохранённого `TargetRigBodySelection`. Она не меняет
selection, generated weights, component overrides, attachments или writer input;
точный окончательный preview всегда возвращает полную геометрию.

Команда **К спине** создаёт строгое one-hot назначение точной deform-кости `Spine_03`,
а **К голове** — `Head`. Оно применяется до bake и не смешивается с автоматическим
nearest-capsule attachment. **Вернуть автоматическое назначение** удаляет override и
снова запускает обычный выбор ближайшей безопасной deform-кости.

`GeneratedSkinningComponentOverrides` хранит fingerprint target rig, fingerprint
donor geometry/topology, полное число компонентов и точную vertex membership каждого
назначенного компонента. Core отклоняет stale fingerprint, неверное число, duplicate,
отсутствующий компонент, изменённую membership или отсутствие единственной точной
кости назначения. Identity override намеренно не зависит от alignment и texture
payload, поэтому GUI сохраняет его при изменении позы, uniform scale/translation,
переключении режимов редактора/портирования и текстур; смена target, donor или полный
сброс очищают состояние.

Override разрешён только для detached-компонента. Если волосы, одежда или другая
деталь сварены вершинами/треугольниками с выбранной smooth body surface, инструмент
не может выделить их как целое и не превращает часть smooth surface в rigid mesh.
Существующий записанный SMO также не меняется задним числом: после изменения алгоритма
весов или ручных назначений его нужно заново подготовить и записать.

Режим не удаляет wings, одежду, волосы или неизвестные части. Любая такая rigid
assignment остаётся в `Attachments`; при близких кандидатах выдаётся дополнительное
предупреждение. `RequiresConfirmation` всегда равен `true`, даже если attachments
нет: алгоритм не может доказать front/back, отсутствие mirror и качество weights
в предельной анимации. Если topology или fit неоднозначны, он завершает подготовку
ошибкой вместо выбора «на глаз».

`TargetRigAutomaticPoseFitter` работает после отдельного этапа размещения и принимает
только применённые uniform scale/translation. Его `SelectBody` выполняет только
детерминированный выбор lower-body и torso/arms компонентов; `Fit` использует тот же
selection, извлекает donor landmarks и затем оптимизирует шесть симметричных body-
параметров режима **Человек**: подъём и forward-направление рук, сгиб локтей,
разведение и сгиб ног, наклон корпуса. Благодаря разделению этих операций GUI может
применить текущую ручную или нейтральную позу и подготовить веса без предварительного
AutoFit. Шея вперёд/назад доступна в том же режиме как отдельная ручная поправка и
пока не входит в objective Auto. Руки и ноги решаются как двухсегментные цепочки с
неизменными длинами; предпочтительное направление сгиба рук направляет локти в
анатомически ожидаемую сторону. При pending или плохо совпавшем alignment Auto не
должен скрыто менять размещение: GUI блокирует запуск либо показывает диагностическое
предупреждение/ошибку score.

Public `TargetRigBodySelection` хранит исходную принадлежность вершин, counts, area и
aligned bounds каждого выбранного компонента, а также fingerprints точных target rig,
donor geometry/topology и alignment. `GeneratedSkinningPreparer` повторно проверяет
эти данные, отсутствие overlap и конечность bounds перед расчётом весов. Texture
payload не входит в geometry fingerprint, поэтому внешний texture override не делает
валидный selection устаревшим.

### Material-контракт Alpha/Blend

`FinalBlendOp` сериализует отдельную операцию/enum и не является набором битовых
флагов. В частности, `FinalBlendOp = 4` нельзя считать обычным Alpha по биту `0x4`:
это подтверждённое glow/effect-семейство, запрещённое для skinned body-atlas.

Production-эталон обычной skinned texture-alpha surface — нативный comparator
`Minautor.smo`. Он подтверждает не отдельный байт, а полный связанный контракт:

- `FinalBlendOp = 2`;
- `MaterialRenderStates = [0, 0, 1, 0, 1, 1, 3, 0, 4, 0, 6]`;
- `LayerTextureStates = [0, 3, 3, 0, 0, 4278190080, 2, 0, 0]`;
- consuming `spSkin.AlphaSortEnable = 1` и `Priority = 1`;
- vertex diffuse равен `0xFF000000`;
- каждый самостоятельный source renderable находится в отдельной material-bearing
  цепочке `spSkin → material → mesh` с reference на общую character texture, а не
  смешивается с opaque geometry или другим renderable одним material consumer;
- material-less mesh допустим только как palette/ushort continuation того же
  исходного renderable.

Тиара `bloom_princess.smo` подтверждает полезную структуру маленькой локальной
alpha-ветви, но её старые `MaterialRenderStates[5] = 0` и
`spSkin.AlphaSortEnable = 0` не являются production-контрактом больших
skinned-крыльев. Игровое наблюдение показывает, что с таким состоянием крупные
крылья могут перекрываться более далёкими alpha-поверхностями листвы и уровня.
Поэтому Princess tuple не переносится на них автоматически.

Этот tuple не является универсальным определением всех skinned materials. В частности,
произвольный `FinalBlendOp = 2` без точного consumer state и прозрачных texels,
фактически покрытых UV его triangles, не классифицируется как Alpha. В корпусе есть
отдельные effect families с другими companion states: `FinalBlendOp = 4` подтверждён
для glow/effect, а встречающийся у `IceWorm`/`Yeti` `FinalBlendOp = 6` не является
универсальной заменой состояния персонажа.

PNG Alpha не является семантикой материала. `mat3`/глаза и `mat4`/рот Layla содержат
полезный RGB под исходным `A = 0`, поэтому они требуют явного source-bound профиля
`OpaqueOverlay`, а не production alpha-состояния. Профиль применяется одинаково в Analyze,
preview и writer; неполная texture group, устаревший donor fingerprint, повторный или
недопустимый mesh key отклоняются. До premultiplied resize выбранной texture group
весь Alpha принудительно становится `255`, сохраняя скрытый RGB. Отдельные post-body
ветви используют канонический tuple глаз `bloom_jeans.smo`: `FinalBlendOp = 0`,
`MaterialRenderStates = [0, 0, 1, 0, 1, 1, 3, 0, 4, 0, 6]`, те же
`LayerTextureStates`, `AlphaSortEnable = 0`, `Priority = 1` и vertex diffuse
`0xFFFFFFFF`. Белый diffuse совпадает с generated retained body, который writer
получает для OBJ без vertex colors, и не создаёт отдельную цветовую модуляцию лица.
Нормали накладок этим контрактом не меняются: они не сплющиваются, не
перенаправляются и проходят существующую генерацию/импорт нормалей. Default `Auto`
остаётся прежним.

Production writer Layla разделяет один RGBA-atlas на consumers по triangles. Ровно
2 714 opaque body triangles остаются на существующих opaque `spSkin/material`
branches target без изменения их material state. Два отдельных opaque-overlay runs
получают `mat3` (72) и `mat4` (52), а 70 настоящих alpha triangles помещаются в три
material-bearing runs с production-состоянием `Minautor.smo`: `mat5` (34),
`mat7` (34), `mat6` (2).
Распределение palettes выполняется только внутри одного исходного renderable;
material-less continuation допустим лишь для его собственного palette/ushort split.
Для всех частей используются текущие generated weights, target inverse-bind
matrices и 16-костные palettes. Ранний importer уже сохранял material groups
раздельно, но его Head-only weights и `FinalBlendOp = 4` были неверны; production
split не возвращает эти ограничения.

`mat6` остаётся отдельным двухтреугольным alpha overlay подвески. Opaque body branch
за ним продолжает участвовать в depth/occlusion, поэтому прозрачное окно подвески не
должно открывать внутреннюю геометрию из-за перевода всего тела в alpha consumer.

Rigid multi-material packer сам строит one-bone `spSkin`, поэтому прежний кандидат
`FinalBlendOp = 6`/`RS[8] = 2` для него не подтверждён. Он принимает только opaque
frames и повторно проверяет отсутствие Alpha непосредственно перед записью.

Exact tuple/pixel, writer round-trip и native load доказывают структуру и загрузку,
но не рендеринг. Viewer и Blender также не воспроизводят игровую реализацию blend,
lighting, depth и сортировки: один объединённый alpha mesh может выглядеть у них
правильно и терять coplanar face/ornament overlays в игре. Orbit камеры при
фиксированном world-light Viewer может скрыть углозависимую ошибку. Визуальный
паритет нового контракта не считается подтверждённым до пользовательского теста
вновь созданного SMO непосредственно в игре.

На контрольных `local-data/Лейла/Model.obj` и `Model.fbx` selector выбирает две body-
поверхности из 43. Они обе получают smooth generated weights, остальные 41 компонент,
включая восемь частей крыльев, проходят прежний rigid-attachment path. OBJ и FBX после
соответствующего unit scale дают эквивалентную позу и selection. Семь material groups
могут упаковываться в один 256×256 RGBA-atlas основного target texture group независимо
от того, содержат ли они Alpha: opaque body, opaque overlays и alpha triangles
обслуживаются описанными выше раздельными consumers с общей texture reference. Для
контрольной Layla `mat3`/`mat4` выбираются как **Непрозрачная накладка**, а
`mat5`/`mat6`/`mat7` остаются в **Авто**. Palette allocator
использует точное maximal-set/backtracking разбиение; если четыре влияния объективно не
помещаются, mode 3 повторяет подготовку с максимумом 3, затем 2. Для Layla выбран
максимум 3 влияния. Ранее созданные диагностические результаты прошли strict
verification, exact tuple/pixel round-trip и native FastGeneric load, но эти
проверки не подтверждают игровой blend. Цена
фиксированного atlas — возможное уменьшение исходных изображений; atlas/Alpha и
деформацию в анимациях необходимо проверять вручную в самой игре.

Если GLB/FBX rig не удаётся разобрать, reader может повторно загрузить meshes,
materials и textures через geometry-only fallback. Причина отказа rig остаётся
видимой, а некорректные bones/weights не передаются ни в режим 1, ни в режим 2.
Такой fallback делает геометрию подходящим входом для режима 3, но сам по себе не
считает веса и не означает успешную совместимость.

Auto сначала проверяет строгий режим 1, затем safety-анализ режима 2. Если режим 2
не может однозначно сопоставить все активные donor weights, Auto рекомендует режим 3
и тем самым отказывается от использования donor rig; это не отменяет консервативных
проверок topology, ориентации и fit самого режима 3.

GUI показывает одно главное действие или реальный blocker рядом с preview, а полную
диагностику и список rigid attachments держит в свёрнутой ограниченной по высоте секции.
Список **Текстуры для импорта** расположен сразу после выбора donor, до placement,
подготовки weights и автоматической нарезки, чтобы exact material mapping уже входил
во все последующие планы.
Редактирование scale/translation создаёт pending
alignment и блокирует plan/save до явного **Применить**; успешное применение создаёт
новую ревизию preparation и сбрасывает прежнее подтверждение. Серый исходный target
mesh в viewport проходит настоящее linear-blend skinning текущей fitting pose, поэтому
можно сравнить его с оранжевым donor до генерации/записи, а не смотреть на статичную
bind-модель. При fitting pose donor preview использует `FittingPreviewScene`, а
plan/save — запечённый обратно в canonical target bind `PreparedScene`. Поскольку Core
всегда возвращает `RequiresConfirmation = true`,
GUI разрешает подтверждение только после фактического показа канонического
`PreparedScene` текущей ревизии хотя бы один раз. Отдельная кнопка сама переключает
viewport с `FittingPreviewScene` на точный итог; новая правка позы сбрасывает отметку
просмотра и подтверждение. Plan/save остаются заблокированными до просмотра и явной
галочки. Режим 3 допускает
только local bone rotations: root rotation/translation fail-loud отклоняются, так
как donor alignment остаётся в canonical target space. Scale/translation уже входят
в обе подготовленные сцены: viewport использует identity transform поверх них, и
`PreservePreparedGeometry` writer также получает identity, исключая double-transform.

Непосредственно перед **Автоматической нарезкой** GUI предлагает отдельный
окончательный текстурированный preview. Для режимов 1–3 он вызывает тот же
`PrepareGeometryPreview`/plan preparation path, что writer, и визуализирует его
полную `ImportedScene` с фактическим atlas, переназначенными UV и импортируемым image
payload. Серый target, skeleton overlay и временная изоляция rigid-компонентов в этом
представлении скрываются. При **Оставить текстуры исходного SMO** donor texture не
поступает writer-у, поэтому эта кнопка недоступна; проверка остаётся на существующем
явном одноцветном canonical review. До зачёта текущей ревизии review-кнопка остаётся
доступной даже если canonical scene уже видна, чтобы простой показ не считался
неявным подтверждением. Legacy rigid `matN` также не получает final textured preview:
его сырой viewport не выполняет packer/writer и не является точным итогом.

Если выбранный GLB/FBX уже содержит skinning целиком или лишь в части meshes, режим
3 повторно читает весь исходный файл geometry-only: повреждённые bones/weights не
используются, но отдельные meshes не отбрасываются. Внешние texture overrides
сохраняются. Папка `matN` является только rigid workflow: обнаруженный bundle в
режиме 3 полностью игнорируется, его frames не показываются, а выбор папки отключён.
Изображения следует добавлять пакетно через **Добавить файлы…**; их material mapping
должен быть однозначным относительно полной geometry-only сцены.

## Неизменяемый target rig и числовая подгонка позы

Ни один из трёх режимов не переписывает target node/service graph, parent topology
или animation logic. Donor graph не сериализуется. Подготовка меняет mesh geometry,
weights и reference-only palettes для существующих target nodes; исходные bind
матрицы и длины костей остаются игровыми.

GUI создаёт отдельную transient-модель `TargetRigFittingPose` и один общий draft для
обоих представлений редактора. По умолчанию открывается режим **Суставы**. В нём
можно выбрать deform joint из списка или щелчком по его screen-space marker в
preview; три ползунка задают абсолютные local Euler angles X/Y/Z от −180° до +180°.
Выбранный сустав подсвечивается красным. Euler angles существуют только как UI-
представление: каноническое состояние остаётся quaternion-based, поэтому после
обратного преобразования ползунки могут показать другую, но эквивалентную тройку.

Режим **Человек** переводит симметричные high-level параметры подъёма и направления
рук, сгиба локтей, разведения и сгиба ног, наклона корпуса и движения шеи в local
rotations target rig. Его изменение выполняется как rebase поверх общей позы и
сохраняет накопленные ручные коррекции, а не пересоздаёт pose с нуля. Поэтому
переключение режимов не меняет snapshot: результат **Человека** сразу виден в
абсолютных ползунках **Суставов**, и наоборот ручная правка остаётся частью той же
позы. Предпочтительное направление two-bone решения рук исправлено так, чтобы локти
сгибались анатомически, тогда как направление коленей осталось прежним. Параметр
движения шеи поворачивает `Neck` и добавляет обратную bind-local rotation к `Head`,
сохраняя world-facing направление головы; уже внесённая отдельная коррекция `Head`
компонуется поверх неё и не стирается при rebase.

Любое изменение ползунка показывает live draft, а **Применить** фиксирует общий
snapshot. Неприменённый draft считается pending и блокирует plan/save. Модель не
владеет `SmoDocument`, отвергает non-uniform scale/shear и после каждого расчёта
проверяет длину каждой parent-child связи. Snapshot привязан к точным bytes target
SMO; поза от другой модели блокируется. Кнопка **Применить** не зависит от выполнения
AutoFit: при отсутствии auto-draft она rebase-ит текущие Human controls, а body
selection разрешается отдельно через `SelectBody`.

Skeleton overlay строится в screen space после проекции текущей fitting pose и
рисуется поверх mesh. Поэтому deform-кости остаются видимыми внутри непрозрачной
геометрии, выбранный joint можно надёжно подсветить и использовать для click-
selection. Это только отображение и hit testing: overlay не меняет target graph,
world transforms, bind matrices или данные, поступающие writer-у.

В режиме 2 можно дополнительно задать uniform donor alignment и включить ручной
weights-only путь. Root alignment там относится только к временной fitting pose.
В режиме 3 root rotation/translation отключены: абсолютный donor geometry alignment
уже задаёт canonical target space, поэтому смешивать его с ещё одним root space без
явного контракта нельзя. Этот alignment не является transform target root: skeleton,
parent-child lengths, bind matrices и IBM остаются исходными. Оба режима показывают
fitting geometry до сохранения и запекают её positions/normals обратно в неизменный
canonical bind; writer получает canonical target names и IBM.

Это constrained editor с симметричным two-bone IK и absolute per-joint rotations,
но не полноценный rigging viewport. Click-selection суставов и detached-компонентов
уже поддерживается, однако gizmos для прямого перетаскивания/вращения костей пока
отсутствуют. Euler sliders подвержены обычной неоднозначности и gimbal lock около
вырожденных ориентаций; quaternion pose при этом остаётся валидной. Donor должен быть
upright, Y-up и same-facing; Auto не доказывает корректность front/back. Target
animation graph сохраняется, но качество skin-деформации во всех SAN-клипах необходимо
проверить вручную в игре. Будущие gizmos, anchors и подсветка низкой уверенности также
не должны менять target graph, canonical bind, IBM или длины костей.
