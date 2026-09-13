# PC сцена: регистрация, дерево узлов и кадр анимации

## Закрытая связь кадра

`spPCApp4C2D60 →spEngineCore41CD50 →spTaskTimer54/90 →spAnimationManager4535A0`
`→spActor/SAN/node local →spSceneManager45A7D0 →scene.systemRoot14 virtual30(0)`
`→spNode421420 →cached world PRS`.

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

## `spScene`

Class/base IDs `6F927C11/44DE07FD`: это `spNamedObject`.
Registration75FE20, initializer6D3790(CALL6D37B0), factory45EBC0, ctor45EA10,
dtor45E5D0/deleting45EB50, clone45EC20. Exact54/vtable6E7358:
`45EB50 5B7A00 45EC20 413120 45E720 408350 408370`.

Ctor сам добавляет borrowed `this` в конец списка SceneManager, затем:

- создаёт actual `spNode` через421E20 и удерживает одним intrusive reference;
- вызывает45A970(root,scene), задаёт имя `System Root`, добавляет root flag100;
- создаёт четыре собственных подсистемы; это не guessed placeholders:

| Scene field | Зарегистрированная original class | Size |
| ---: | --- | ---: |
| 28 | spPCLensFlareManager | 38 |
| 2C | spPCProjectionManager | 24 |
| 30 | spSkyBoxManager | 24 |
| 34 | spLightManager | 24 |

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

По45A970/45AB30 уже видны branches для `spRenderNode603625D0`,
`spSkyBox7A7124AF`, `spLight72444900`, `spPartitionSystem912CC341`,
`spOcclusionVolume43D24430` и collision objects. Их registrations/culling
нужно разобрать перед полноценным portable scene attachment, а не пропускать.

## Default scene/camera и timer wiring41C300

Core сначала создаёт scene в18, вызывает45D850. При false формирует original
diagnostic с source `spEngineCore.cpp:102`, сообщает ошибку и возвращает0:
созданная сцена остаётся, камера/таймеры не настраиваются, rollback отсутствует.

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
