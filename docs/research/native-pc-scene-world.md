# PC сцена: регистрация, дерево узлов и кадр анимации

Checkpoint 6 цикла 5/6 сентября 2026 года, продолжающегося до10:00 МСК.
Приоритет — реальная PC цепочка SMO/SAN, без запуска игры, GPU и PS2.
Контрольный EXE: `local-data/pc-pristine/WinxClub.exe`, SHA256
`3F022480BF55045DA4BF692E4BC8862ED38FC024E8A964A558FBDFDF646DFC4F`.

## Закрытая связь кадра

`spPCApp4C2D60 →spEngineCore41CD50 →spTaskTimer54/90 →spAnimationManager4535A0`
`→spActor/SAN/node local →spSceneManager45A7D0 →scene.systemRoot14 virtual30(0)`
`→spNode421420 →cached world PRS`.

`probe_pc_animation_scene_frame.py` исполняет эту последовательность целиком:
создаёт actual default scene/camera, связывает таймеры **через41C300**, добавляет
animated nodes **через421A60**, затем вызывает только original app frame.
Три последовательных шага дают samples `.25/.5/.75`; world position учитывает
сдвиг System Root. **Отдельного вызова UpdateWorld из теста больше нет**.
При удалении сцены её ссылки отпускаются, но actor продолжает держать свои узлы.

Explicit seams остаются: память engine вместо protected ctor, полная
scene-initialize45D850, остальные update managers, events, OS foreground и
graphics. Это не испытание нативного GPU render или всего resource loader.

## `spSceneManager`

Class/base IDs `67419388/415352A1`, registration75FCA0, initializer6D36D0.
Factory45ADF0 разрешается в13DEBF0, вызывает **constructor45AD50→13CD6C0**.
Важно: соседний **45ACE0 — не constructor**, а поиск сцены для присоединяемого
node по его ultimate root (`→481C80`, helper420B20).

Exact24 layout: base0..F, singleton support10(vtable6E7150), untouched allocator14,
owned list sentinel18, count1C, borrowed currentScene20. List node12:
next0/previous4/borrowedScene8. Factory публикует singleton75DB90.
Vtable6E7154 содержит семь root slots, без local Update virtual:
`45ADD0 5B7A00 45AE50 40ECE0 45ACC0 408350 408370`.

Update45A7D0 идёт в порядке списка, записывает scene20, вызывает root14
virtual30 с **нулём**, затем читает next **после вызова**. Не проверяет scene24/25
или Node Enabled200. В конце обнуляет currentScene, в том числе при пустом списке.

Dtor45AC70/45ADD0 освобождает list nodes/sentinel, **не сами сцены**; global
очищается без проверки, этот ли экземпляр был текущим. Clone45AE50 создаёт
новый singleton с пустым списком, регистрирует пару и использует base no-op copy.
Ни сцены, ни currentScene не копируются. Arbitrary callback deletion/reentry
не объявляются безопасными из-за одного post-call next read.

## `spScene`

Class/base IDs `6F927C11/44DE07FD`: это `spNamedObject`.
Registration75FE20, initializer6D3790(CALL6D37B0), factory45EBC0, ctor45EA10,
dtor45E5D0/deleting45EB50, clone45EC20. Exact54/vtable6E7358:
`45EB50 5B7A00 45EC20 413120 45E720 408350 408370`.

Ctor сам добавляет borrowed `this` в конец списка SceneManager, затем:

- создаёт actual `spNode` через421E20 и удерживает одним intrusive reference;
- вызывает45A970(root,scene), задаёт имя `System Root`, добавляет root flag100;
- создаёт четыре собственных подсистемы; это не guessed placeholders:

| Scene field | Зарегистрированная original class | Factory | Size |
|---:|---|---:|---:|
|28|spPCLensFlareManager|4C7240|38|
|2C|spPCProjectionManager|4C5CE0|24|
|30|spSkyBoxManager|48DC80|24|
|34|spLightManager|46AB80|24|

LightManager20 получает borrowed scene. Все четыре original ctor/dtor исполнены,
но их rendering algorithms этим не считаются изученными/перенесёнными.

Scene14 — system root; collection18..23 initially0 — borrowed RenderNode
first/last/count (закрыто checkpoint7, links128/12C). Byte24 default0 выбирает
viewport depth range в camera4284B0;
byte25 default1 используется первой границей graphics41C460. Это разные флаги.
38 — partition-system reference (getter45D930); 3C — fallback partition,
создаваемый initialize45D850;40 visibility mark (ctor0, writer46D270);
44 untouched vector allocator;
48/4C/50 — sorted object/distance pairs для45EC70. Padding26/27 untouched.

[Checkpoint12](native-pc-partition-runtime.md) установил concrete fallback
PartitionSystem1D8→PartitionNode84/ZoneC8 graph и разные direct/intrusive/
borrowed relationships. Whole SceneInit не приписывается этим отдельным
lifetime probes; обязательный VisibilityManager75E1B0 ещё исследуется.

Dtor сначала45AB30(root,scene), отпускает root, удаляет свою list entry
через4CDED0→458C20, затем удаляет четыре owned managers, sorted buffer,
intrusive references3C/38 и inherited name. Поэтому manager должен пережить
native сцены; произвольный порядок singleton replacement не поддерживается
одними лишь валидными pointers. Scene clone payload-copy отдельно ещё не
исполнялся: vtable показывает inherited name-copy, не full deep scene copy.

## Присоединение узлов — не просто `parent=...`

Original421A60 сначала удерживает ребёнка, добавляет owned list entry,
отсоединяет прежнего родителя при необходимости, ставит parent2C и dirty bits5.
При bit100 у нового родителя распространяет его на subtree, затем вызывает
SceneManager45ACE0. Этот helper находит сцену по System Root, а45A970 рекурсивно
назначает node.scene3C и выполняет type-specific registrations.

Same-parent attach — early no-op. Перенос whole subtree между двумя сценами
проверен native probe: old parent count0, new parent count1, все scene links
обновлены, world transform наследует нового родителя. Teardown освобождает
всю native owned subtree. Portable `spNode::AttachChildForAnalysis` пока имеет
более узкий контракт (не переносит уже attached node и не ведёт scene registry);
**он не объявляется эквивалентным всей найденной native операции**.

По45A970/45AB30 уже видны branches для `spRenderNode603625D0`,
`spSkyBox7A7124AF`, `spLight72444900`, `spPartitionSystem912CC341`,
`spOcclusionVolume43D24430` и collision objects. Их registrations/culling
нужно разобрать перед полноценным portable scene attachment, а не пропускать.

## Default scene/camera и timer wiring41C300

Core сначала создаёт scene в18, вызывает45D850. При false формирует original
diagnostic с source `spEngineCore.cpp:102`, сообщает ошибку и возвращает0:
созданная сцена остаётся, камера/таймеры не настраиваются, rollback отсутствует.

При success создаётся **spDXCamera4A9120**, удерживается reference1, имя
`Default Camera`. Этот stage **не** добавляет камеру к root и не активирует
engine camera vector. Root timer54 получает active1/relative1; child90.source2C
становится54, child добавляется в конец double-linked списка. Это закрывает
timer10=previous,14=next,30=head,34=tail,38=count. Проверены пустой и непустой
список. Ни Start, ни timestamp initialization здесь не вызываются.

Portable `AppendClockChildForAnalysis` переносит append/source replacement,
с дополнительными host cycle/duplicate bounds; произвольный external reparent
таймера и native detach остаются за границей. Старый SetChildren — literal seam.

## Исправление PC ABI камеры

Обе original factories (spDXCamera4A9120 и spCameraData41A3F0) дают exact238.
Ctor428690 и фактические аргументы matrix apply427D40 доказывают:
**view startsCC, projection10C, basis14C**. Старые D4/114/154 ошибочны.
Region170..187 — viewport storage, до configure/render не инициализируется.

SetViewAngle427DA0 очищает serialized2D byteC8, но не projection branch231;
source setter исправлен. Actual view apply идёт раньше projection и при false
останавливает вызовы. Projection branch426F10 обновляет cache in-place:
orthographic пишет только cells0/5/10/14, не возвращает M23/M33 к identity.
Старый portable `BuildProjectionMatrixForAnalysis` явно оставлен **fresh-matrix
utility**, а не выдан за faithful cached transition. Полный camera runtime —
следующий связный участок после scene registration.

## Проверки и границы

- scene lifecycle/list/world/native reparent: **80/80**;
- core scene/camera/timer setup success/append/failure: **32/32**;
- actual app/timers/SAN/actor/owned scene/world: **39/39**;
- camera factories/cache/renderer arguments: **32/32**;
- PC static registration/call/ABI anchors: **29/29**;
- portable timer **34/34**, прежний timer differential **1968/1968**;
- CTest **12/12**, включая исправление camera setter и ABI asserts.

Seams и общие bounded limits прежние: 100k instructions/2s на guest call,
30s на отдельный Python child. Сцена, Node, четыре subsystem lifecycles и
camera выполняются в guest; host OS/GPU не вызываются. Все tracked native
allocations normal probes освобождены. Initial bare scout дошёл до неинициализированного
string manager/diagnostic import; использован прежний explicit name-owner seam.
Camera destructor требует engine singleton: с absent engine попытался вызвать
уже известный bounded protected ctor; исправлена **fixture dependency**, не EXE.
Первый static camera→scene CALL anchor был на preceding push; corrected428574.

Source класса `spScene`/`spSceneManager` в этом checkpoint ещё не выдаётся за
готовый: сохранены exact ABI и исполнимые original probes. Rendering/initialize,
typed registration side effects и соответствующий portable перенос продолжаются.

Последующие PC checkpoints закрыли [SceneInit/partition transfer](native-pc-visibility-runtime.md)
и [целый SceneRender на bounded fixtures](native-pc-scene-render-runtime.md):
native67 checks, actual Shadow/Lens empty paths, camera/support failure gates.
Уточнение: notification1A находится внутри Debug18 tail, не вызывается после
каждого кадра. Полный startup/Visibility ctor/occluder/portal/GPU и portable
Scene по-прежнему отдельно не объявлены готовыми.
