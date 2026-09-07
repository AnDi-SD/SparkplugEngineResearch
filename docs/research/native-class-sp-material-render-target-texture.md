# `spMaterialTexture` и material render-target textures

Статус: подтверждены RTTI-граф, platform layouts, factory/clone/lifetime,
texture-state и UV relationships, третий список `spRenderTargetManager`, выбор
target/fallback, recursion guard, camera/cubemap runtime state и полная
serializer grammar редких material-layer ветвей. Реальный render callback до
камеры, scene traversal и renderer slots пока остаётся следующей зависимостью.

## Иерархия

| Класс | Class ID | Registered base | Factory |
|---|---:|---|---|
| `spMaterialTexture` | `0x694E6975` | `spBaseObject` | concrete |
| `spMaterialRenderTargetTexture` | `0x535D1473` | `spMaterialTexture` | null |
| `spMaterialCameraViewTexture` | `0x34EF51B9` | `spMaterialRenderTargetTexture` | concrete |
| `spMaterialCubeMapTexture` | `0x1C3B499A` | `spMaterialRenderTargetTexture` | concrete |

Общий render-target класс добавляет два pure slots. Camera-view и cube-map
заполняют оба; один slot запускает соответствующий render path, второй создаёт
копию конкретного texture object. Portable API не присваивает им якобы
оригинальные сигнатуры: он сохраняет только доказанное различие render-kind и
copy semantics.

## Layout

Platform-разница начинается внутри `spMaterialTexture`: PC renderer имеет
девять texture states, PS2 — двенадцать.

| Поле | PC | PS2 |
|---|---:|---:|
| texture-state block | `+0x10`, `9 * u32` | `+0x10`, `12 * u32` |
| fallback `spTexture*` | `+0x34` | `+0x40` |
| controller slot A | `+0x38`: **UV**, PC field12 | `+0x44`: old animation label needs independent recheck |
| static UV matrix, 9 floats | `+0x3C` | `+0x48` |
| has-static-UV byte | `+0x60` | `+0x6C` |
| controller slot B | `+0x64`: **AnimTex**, PC field11 | `+0x70`: old UV label needs independent recheck |
| common texture extent | actual allocation **`0x68`** (checkpoint12) | exact `0x80` |

PC follow-up correction: original477985 loads class16FB0E47 (spAnimTexController),
then47799D calls476680 which writes64 and controller24. Original4779DE loads
class1C0053D6 (spUVController), then4779F6 calls467D90 which writes38 and invokes
UV binder4346C0. Previous PC labels38/64 were reversed. Clone/destructor addresses
stay valid; their semantic labels must follow these actual typed callers.
PS2 labels are not automatically corrected or credited from PC evidence.

Historical PS2 interpretation below requires that independent recheck:
PS2 update `0x001732E0` связывает controller и UV-поля с runtime path:
animation controller по `+0x44` при необходимости обновляется, а static matrix
по `+0x48` передаётся вместе с texture-stage index в renderer slot `23`.
UV controller по `+0x70` затем также получает update callback.

`spMaterialRenderTargetTexture` продолжает этот layout:

PC checkpoint12: word68 **не принадлежит common MaterialTexture**, actual
StdLayer nested factory выделяет104bytes. В historical target layout он
оставлен как derived-only opaque68; роль и полные target constructors требуют
отдельной проверки. Остальные target offsets ниже не пересчитываются по догадке.

| Поле | PC | PS2 | Default |
|---|---:|---:|---:|
| max recursion | `+0x6C` | `+0x80` | `1` |
| current recursion | `+0x70` | `+0x84` | `0` |
| width | `+0x74` | `+0x88` | `256` |
| height | `+0x78` | `+0x8C` | `256` |
| `eTBPixelFormat` | `+0x7C` | `+0x90` | `0` |
| per-pass target vector | `+0x80` | `+0x94` | manager-sized |
| common target extent | observed `0x8C` | exact `0xA0` |

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

1. проверяет `currentRecursion < maxRecursion`;
2. разрешает camera name, иначе использует default camera;
3. выбирает текущий manager slot и при необходимости создаёт ordinary target;
4. привязывает target, рендерит camera, снимает target;
5. уменьшает recursion depth на выходе.

Cube method `0x00171620` повторяет guard, разрешает source `spRenderNode`
(`0x603625D0`), переносит transform в свою camera и обрабатывает заданное число
граней начиная с current face modulo six. Serialized faces-per-tick loader
нормализует `0 -> 1`, `>= 7 -> 6`.

Portable слой пока не вызывает renderer/scene: соответствующие сигнатуры не
угаданы. Он уже предоставляет конфигурацию, guard, face progression и точку,
где следующая реконструкция сможет подключить настоящий camera render.

## Material serializer

Редкие ветви теперь разобраны полностью на уровне wire grammar:

| Layer | ID | Payload после field 4 |
|---|---:|---|
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

## Основные адреса

| Роль | PC | PS2 |
|---|---:|---:|
| material-target registration | `0x007626D0` | `0x004A98B0` |
| target-or-fallback getter | `0x0048DE90` | `0x00172630` |
| release loop | `0x0048DED0` | concrete vtable path |
| reinit loop | `0x0048DF10` | concrete vtable path |
| common target writer | protected PC path | `0x00192900` |
| common target reader | protected PC path | `0x00193B10` |
| layer writer/reader/index | protected PC paths | `0x00192D90` / `0x00195300` / `0x001956A0` |

## Проверка

```powershell
python -B research\inspect_material_render_target_textures.py
cmake --build .codex-tmp\Sparkplug-build-20260904
ctest --test-dir .codex-tmp\Sparkplug-build-20260904 --output-on-failure
```

Read-only scanner проверяет SHA-256 контрольных executable, RTTI initializers,
PC/PS2 vtables, factory allocation constants, полные hashes ключевых functions,
все шесть layer IDs и surviving diagnostic tokens: `56/56`.

## Осталось неизвестным

1. Original headers/TU и точные имена двух дополнительных virtual slots.
2. PC body за защищёнными factory/copy trampolines и direct allocation camera.
3. Точные camera-manager lookup и renderer target-bind сигнатуры.
4. Полное intrusive ownership fallback/controllers/targets.
5. Реальный consumer movie stream и platform video backend.
6. Проверка редких ветвей на настоящем authored asset: в текущем SMO-корпусе
   они не представлены.
