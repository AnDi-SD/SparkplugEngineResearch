# spMaterialTexture

Описание отдельных известных частей класса. Наличие карточки не означает полного восстановления всех методов.

Общие исходники: [spMaterialTexture](../../../Sparkplug/Code/Sparkplug/spMaterialTexture.h), [spRenderTargetManager](../../../Sparkplug/Code/Sparkplug/spRenderTargetManager.h).

Реальный render callback до камеры, scene traversal и renderer slots пока остаётся следующей зависимостью.

## Иерархия

| Класс | Class ID | Registered base |
| --- | ---: | --- |
| `spMaterialTexture` | `0x694E6975` | `spBaseObject` |
| `spMaterialRenderTargetTexture` | `0x535D1473` | `spMaterialTexture` |
| `spMaterialCameraViewTexture` | `0x34EF51B9` | `spMaterialRenderTargetTexture` |
| `spMaterialCubeMapTexture` | `0x1C3B499A` | `spMaterialRenderTargetTexture` |

Общий render-target класс добавляет два pure slots. Camera-view и cube-map
заполняют оба; один slot запускает соответствующий render path, второй создаёт
копию конкретного texture object. Portable API не присваивает им якобы
оригинальные сигнатуры: он сохраняет только доказанное различие render-kind и
copy semantics.

## Layout

Platform-разница начинается внутри `spMaterialTexture`: PC renderer имеет
девять texture states, PS2 — двенадцать.

Historical PS2 interpretation below requires that independent recheck:
PS2 update `0x001732E0` связывает controller и UV-поля с runtime path:
animation controller по `+0x44` при необходимости обновляется, а static matrix
по `+0x48` передаётся вместе с texture-stage index в renderer slot `23`.
UV controller по `+0x70` затем также получает update callback.

`spMaterialRenderTargetTexture` продолжает этот layout:

| Поле | Default |
| --- | ---: |
| max recursion | `1` |
| current recursion | `0` |
| width | `256` |
| height | `256` |
| `eTBPixelFormat` | `0` |
| per-pass target vector | manager-sized |

Camera-view добавляет camera name и resolved camera (`+0x8C/+0x90` PC,
`+0xA0/+0xA4` PS2). Cube-map добавляет source render node, owned camera,
faces-per-tick и current face (`+0x8C..+0x98` PC, `+0xA0..+0xAC` PS2).
PS2 factories выделяют соответственно `0x80`, `0xB0`, `0xB0`; PC cube-map
factory подтверждает complete extent `0x9C`. PC camera extent `0x94` остаётся
observed, потому что factory скрыта trampoline-ом.

## Связь с `spRenderTargetManager`

Конструктор общего target-texture регистрирует объект в третьем manager list по
`+0x30` и создаёт vector размером `manager+0x40` (по умолчанию один slot).
`manager+0x3C` — текущий target index. Если он равен slot count, либо slot пуст,
`GetTexture()` возвращает fallback; иначе возвращает backing texture выбранного
`spRenderTarget` по `+0x18`.

Reset manager проходит все три списка. Для material target list он вызывает
отдельные loops release/reinit, которые обходят per-pass vector. Удаление или
очистка target оставляет list node, но деактивирует/null-ит payload — это тот же
контракт, что у ordinary и cube target lists.

Portable реализация воспроизводит lifecycle и выбор texture. Raw pointers в
analysis facade не объявлены точной копией native intrusive ownership: этот
контракт будет завершён вместе с полным `spTexture` backend.

## Camera и cubemap render paths

PS2 camera method `0x00170DC0`:

Portable слой пока не вызывает renderer/scene: соответствующие сигнатуры не
угаданы. Он уже предоставляет конфигурацию, guard, face progression и точку,
где следующая реконструкция сможет подключить настоящий camera render.

## Material serializer

| Layer | ID | Payload после field 4 |
| --- | ---: | --- |
| `spStdLayer` | `0x234C576B` | common texture fields |
| `spEnvironmentMapLayer` | `0x427C7480` | common texture + field `7` UV generation |
| `spCubeEnvMapLayer` | `0x4DED3E44` | common texture fields |
| `spCameraViewLayer` | `0x194613E1` | common texture + `13` target + optional `14` camera name |
| `spMirrorLayer` | `0x46B61C67` | common texture + `13` target + `15` faces-per-tick |
| `spMovieLayer` | `0x075F3EB6` | field `16` movie filename |

Common texture fields: `17` texture states, optional `9` static UV, `10`
fallback/ordinary texture, `11` animation controller, `12` UV controller.
Render-target field `13` содержит четыре `u32` именно в порядке width, height,
pixel format, max recursion. Index pass для camera/mirror использует fallback,
UV-controller и animation-controller; movie branch не индексирует texture
relationships.

Неочевидное, но подтверждённое разделение: `spCubeEnvMapLayer` не является
динамическим cubemap writer-ом. Field `15` принадлежит `spMirrorLayer`, чей
nested texture имеет class ID `spMaterialCubeMapTexture`.
