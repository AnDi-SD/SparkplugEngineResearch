# Independent scene inputs: ordinary unlit cohort

13 сентября 2026. Read-only контракт на основе существующих CP7/10/11/32/36
и общих исходников; новых native emulation/GPU запусков и production edits нет.

**Минимальный подход:** заново читать текущий owner graph и native world PRS,
а материал ограничить exact ordinary single-layer без render-time controllers
и Model callbacks. Собственный packet строится на локальных копиях через общие
CPU helpers. Support Draw/Prepare, Model Render/Pre/Post и controller Update
ради получения packet не вызываются.

Это условно неизменный **внутри пропускаемых render-time producers** путь.
Такие поля всё ещё может менять игра между операциями. Ни StaticMask, ни
runtimeMode, ни несколько одинаковых кадров не доказывают постоянную
неизменность. Last-visible packet не является источником актуального кадра.

## Owner и world: брать текущий результат update

Для поддержанного RenderNode остаются exact inherited support `6DCADC`,
`support=object+B4`, self124, scene3C, живое membership в текущем root/vector20
и уникальная adapter operation identity; derived primary сами по себе не
исключаются. См. [точный owner контракт](winx-remix-rendernode-owner-contract-2026-09-13.md).

| Native input | Правило первой выборки |
|---|---|
| object+B0 flags | Enabled bit200; billboard bits300000 должны быть нулевыми |
| flags bits1/2/4 | Dirty propagation, не счётчик актуального кадра. Для первого packet требовать `(flags&7)==0` после game UpdateWorld |
| object+74/80/8C | Текущие world position[3], scale[3], orientation[9], все finite |
| object+134 bit1 | Lazy render-matrix dirty, отдельно от Node flags |
| object+138/178 | При matrixDirty bit1=0 можно копировать inline world/inverse после проверки pointer identities и freshness |
| object+1B8 | Текущие reciprocal world scale[3], finite; для первого cohort scale ненулевой |
| support+34/+38 | Должны указывать на object+138/+178; custom pointer path пока исключён |

После `spSceneManager45A7D0 → scene.systemRoot14.v30(0)` обновлены Node world
PRS; RenderNode4250F0 рассчитывает reciprocal scale и может только поставить
matrixDirty, не пересчитать render matrices. Когда объект не дошёл до Prepare,
inline138/178 могут оставаться старыми. При dirty нужно рассчитать **свою копию**
матрицы из world PRS, не читать прошлую render cache и не сбрасывать native dirty.

Использовать общие pure CPU helpers:

- [`node_math::Affine`](../../Sparkplug/Analysis/PC/spNodeTransformMath.h),
  native461D70: `Affine(worldPosition, worldOrientation, worldScale)`.
- [`render_node_math::InversePRS`](../../Sparkplug/Analysis/PC/spRenderNodeMath.h),
  native461EB0: `InversePRS(worldPosition, worldOrientation, reciprocalScale)`.
- Существующий порядок показан в
  [`spRenderNode::UpdateRenderMatricesForAnalysis`](../../Sparkplug/Code/Sparkplug/spRenderNode.cpp)
  и [`spNode::GetWorldMatrixForAnalysis`](../../Sparkplug/Code/Sparkplug/spNode.cpp).

Не подменять inverse general inversion: native использует transpose orientation
и reciprocal scale. Helpers прямо обозначены finite-input reconstruction,
без универсального обещания bit-identical x87. Dirty-computed packet сначала
сопоставлять с последующим native Prepare того же текущего owner; clean cache
можно сравнивать побитно. Новый второй алгоритм PRS в adapter не нужен.

**Freshness остаётся обязательной границей:** native UpdateWorld данного scene
должен быть завершён в текущем frame/update sequence до extraction. Одни
нулевые dirty bits этого не доказывают. SceneRender scope и destructor epoch
не являются UpdateWorld epoch. Если hook/trace этого порядка ещё отсутствует,
первый самостоятельный packet требует такого phase witness; переносить весь
Node UpdateWorld на наши временные объекты нельзя — там descendants, bounds,
collision/light/partition notifications. Animated/Bone flags800/1000 и Static400
не доказывают наличие/отсутствие внешних writers. Billboard исключён, поскольку
4250F0 ставит dirty для следующего кадра уже после capture старых flags.

## Exact Model: исключить render-time callbacks и alpha ordering

Model primary6EAA58, mesh58 с текущим qualified native generation. Render v24
479DC0, pre v20=423FD0, post v28=4240D0; при установленных хуках сравнивать с
ожидаемыми принадлежащими adapter slots, не с устаревшим original word.

| Поля Model | Guard / смысл |
|---|---|
| direct callbacks2C/30 | Оба NULL |
| post vector34: begin38/end3C/cap40 | Проверить bounds; begin==end |
| pre vector44: begin48/end4C/cap50 | Проверить bounds; begin==end |
| enable bytes54/55 | Допускаются с пустыми vectors; nonempty даже disabled пока исключить |
| material20 | Ненулевой exact DXMaterial; сначала исключить fallback selection |
| alphaSortEnabled18, priority1C | Alpha sorting не путать с blend/material mode |
| runtimeMode14 | Отдельный classifier для nine-bucket queue; это **не** lighting state8 |
| projectionGroup5C, raw28, fog24 | Не приписывать им имена/значения shader modes; текущий fog остаётся отдельным guard |

Первая выборка: pass.finalBlend10==0. Тогда условие alpha enqueue из CP11
ложно независимо от Model18: оно требует nonzero pass10, renderer44=false и
rendererC9D8=true. Оpaque/pass0 снимает необходимость воспроизводить alpha
priority/distance/sort; CPU renderer queue не запускать. Пустые callback groups
нужны даже при direct NULL: у group callbacks другой ABI/return semantics и
они способны менять/удалять entries, материал, mesh или соседние owners.

Для этой PC версии отдельно квалифицировать normal queue selection
`byte[75F8E8]==0`. При нём Model.runtimeMode14 не участвует в general draw;
нет основания требовать у всех Model какой-либо lighting-like mode.
При alternate nine-bucket ветке фазы текущего PC renderer — no-op (CP11);
просто добавить такие модели в independent drawing означало бы изменить
поведение без проверки. C050 обозначает enqueue phase, не неизменность модели.

## Material, pass и texture: нет пропускаемого clock consumer

| Native поле | Guard |
|---|---|
| Material primary/interface | exact DXMaterial6EF264 / +14=6EF238 |
| +48, +4C | passCount1, passes[0] exact MaterialPassLayer |
| +74 | materialColorController NULL |
| +18 state[8] = +38 | raw lighting mode2; effective selected mode также2 |
| Pass+14/+18 | layerCount1, layers[0] exact StdLayer |
| Layer+10 | exact ordinary MaterialTexture6E8440, без derived getter/RTT/camera texture |
| MaterialTexture+38/+64 | UV controller NULL, AnimTex controller NULL |
| MaterialTexture+60 | hasStaticUV=0 |
| MaterialTexture+10 raw9 | source states7/8 через общий mapper дают UV0 / transform disabled; остальные операции в поддержанном диапазоне |
| MaterialTexture+34 | Текущий fallback exact DXTexture; current device/resource identity и transport generation |

В CP34 `45F570→4596B0` проходит слои; layer423460 вызывает texture467B70.
Общий [`spMaterialTexture::UpdateForRenderForAnalysis`](../../Sparkplug/Code/Sparkplug/spMaterialTexture.cpp)
обновляет UV, при наличии UV/static transform отправляет matrix, затем обновляет
AnimTex. При трёх NULL/zero guards выше эти обновления и matrix emission
отсутствуют. **Не заменять NULL проверку disabled-controller flag:** controller
имеет clocks/backlink, а shared controller способен менять другой holder.

DX material install4BE180 копирует colors **до** color update. Mode2 не меняет
colors и отключает lighting/specular (CP32). Поэтому собственный unlit packet
использует тот же `material_channels::UnlitInput(preserveUnlitColor)`, что
действующий backend; material colors не превращаются в diffuse/emission по
догадке. Color controller всё же исключается: его пропуск изменил бы clocks/
shared owner, даже когда данный draw цвета не потребляет. Material runtime70
содержит render-frame bookkeeping и не является proof of current producer.

Уточнение «неизменного» пути: `4BC410` всё равно записывает pass.finalBlend10
в **material state7**. Для собственного packet сделать эту известную подстановку
в локальном material-state copy; native material/cache не модифицировать.

## C188/C1C4: сначала определить фазу и source selection

Начальная independent boundary — **вне активного Model/Support draw**, после
актуального world update; либо отдельный before-ModelPre snapshot для сравнения.
In-draw значение C1C4 нельзя автоматически использовать как outer state.

- C188 materialOverride: для первой выборки outer C188==0. Тогда Pre выбирает
  Model.material20 (или fallback, который здесь исключён), а не чужой C18C.
- Material+6C renderOverride: если nonzero, Pre сохраняет outer C1C4 в global
  7400FC и ставит C1C4=0; Post восстанавливает global. При +6C==0 этого
  переключения нет. Поэтому локально `effectiveC1C4 = material6C ? 0 : outerC1C4`.
  Это не требует записи native byte/global или вызова Pre/Post. Наблюдённый
  C1C4=1 внутри draw не доказывает before/after состояние.
- CP33 перед material-state batch устанавливает source slot0 на selected+18.
  В независимом packet slot0 должен обозначать **нашу текущую local material
  copy**, а не C1C8 от последнего нарисованного Model. Если effectiveC1C4!=0,
  первый cohort требует material selectors `C71C+4*index`, index1..10, равными0.
  При effectiveC1C4==0 они не участвуют. C738 — именно selector final blend7.
- Texture selectors `C748+4*(9*stage+state)` действуют **независимо** от C1C4.
  Для stage0 states1..8 требовать0, а выбранный vertex shader `E454[E474]` —NULL;
  shader-coordinate override пока не воспроизводить. Число допустимых native
  selector/source entries не выдумывать.

Так C1C4=1 с ordinary source0 поддерживается; глобального C1C4==0 cohort нет.
Существующие common helpers: `spDXRenderer::ApplyMaterialStateSetForAnalysis`,
`ApplyMaterialRenderStateForAnalysis`, `ApplyMaterialLightingForAnalysis` в
[`spDXRenderer.h`](../../Sparkplug/Code/SparkplugDX/spDXRenderer.h), и общий
[`ApplyPCTextureStateForAnalysis`](../../Sparkplug/Code/SparkplugDX/spPCTextureStateMapping.h).
Использовать локальные caches/recording sink, а не live native objects и не
параллельную копию таблиц. Полную model pre/post транзакцию эти helpers не
объявляют восстановленной; source-selection boundary выше — явная обвязка.

## Остаток до самостоятельного API submit

1. Нужна проверенная текущая world-update phase и owner membership/epoch fence
   до и после чтения. Removal/reparent/root replacement, bounds и pointers
   сверять в текущем graph; parent callbacks не подавлять.
2. В raw material/texture words нет stage arguments/RESULTARG/TFACTOR, некоторых
   blend/depth/color-write/fog switches. Сохранить квалифицированный D3D bootstrap
   contract, пока точный producer этих inputs не перенесён. Stage0 требует
   TEXTURE/CURRENT, RESULTARG CURRENT и отключённый следующий stage. CP36
   заполняет неиспользуемые stages из default material, поэтому один layer сам
   по себе не доказывает disabled stage1. Fog24==NULL также не доказывает fog off:
   renderer имеет fallback. Нужны настоящие state guards, не нулевые догадки.
3. Texture pointer без актуальных bytes/device generation недостаточен; animated,
   dynamic upload/palette/custom texture resource остаются отдельными границами.
   Unweighted mesh не гарантирует неизменность vertex buffer. Использовать только
   generation с подтверждённым producer/update coverage.
4. Сначала сравнить независимые **свежие** world/material packets с существующим
   native/D3D path того же owner/frame: clean и dirty world, до/после Pre C1C4,
   static-controller-negative, callbacks-negative, source-selector-negative.
   Нынешний material match внутри native draw подтверждает consumed packet,
   но не заменяет проверку нового before-render producer boundary.
5. Direct extraction не выполняет callbacks или clock consumers повторно и
   не даёт права удалить оригинальные producer/update phases игры. Первые
   controller-free offscreen additions не должны воспроизводить последние
   видимые кадры; полностью готовый новый packet строится из текущих owners.

Граница готовности этого документа — контракт inputs и явные guards. Количество
подходящих live объектов, точка world-update witness и независимый API outcome
в этой read-only задаче не измерялись и не объявляются подтверждёнными.
