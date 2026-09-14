# Пакеты Skin для переноса в Remix

`-NativeSkinPackets` сохраняет ограниченные снимки текущей геометрии и
палитры PC Skin. Это собственный формат адаптера, а не формат игры.
Правила допуска основаны на layout, диапазонах draw и времени жизни
ресурсов; имена моделей, уровней и хеши ассетов в них не участвуют.

## Захват

Опция включает существующие native observers и shader audit. Проверяются
владельцы Skin/scene/camera, исходные VB/IB, точное совпадение с копией
загрузки D3D, декларация и подтверждённая история создания/освобождения
ресурсов. Последняя проверка перечитывает заимствованные данные и палитру.
В пакет попадают собственные копии; указатели и COM references не сохраняются.

Поддерживаемый контракт: B1–B4, до 16 affine-матриц, FLOAT3 position/normal,
явные веса, FLOAT4 indices, необязательные COLOR0 и FLOAT2 UV0, INDEX16,
triangle list/strip. Отрицательные веса и сумма, отличная от 1, сохраняются.
Четвёртые компоненты position и normal равны 1 согласно входной декларации.
Другие варианты требуют отдельного установления shader-контракта.

Пакет содержит только вершины, используемые текущим draw. Перенумерация
идёт по первому обращению; порядок треугольников, winding strip и
вырожденные треугольники сохраняются. Палитра копируется из оригинала
после проверки общей восстановленной формулой; её округление не заменяется
результатом этой формулы.

На один запуск: максимум 64 файла и 32 МиБ данных пакетов, по 3 снимка на
текущий набор scene/skin/mesh/generation. Один пакет ограничен 65536 вершинами,
32768 треугольниками и 8 МиБ. Запись создаёт новые файлы исключительно;
коллизия имени или неполная запись останавливает экспорт. Повторный опыт
использует новый `-Name`.

`skin-packets/packets.jsonl` связывает файл с native call/submission.
Запись `draw_result`, сделанная после возврата реального D3D draw, отдельно
подтверждает bound VB/IB/declaration, аргументы draw, bytecode ID и
снимок float-констант `packet-NNNN-draw-NNNNNN.constants`.
Это ещё не доказательство завершения GPU-команды. У пакета остаются
`shaderVerified:false` и `materialCaptured:false`: соответствие выбранной
программе и материалу проверяется отдельно. Захват сам ничего не передаёт
через Remix API и не подавляет исходный draw.

Системный D3D9 доступен с `-DebugMenu -Backend system -NativeSkinPackets`.
В этом режиме scene observer не расширяет исходную выборку объектов.
`-GameDirectory` позволяет использовать отдельную подготовленную копию
игры внутри workspace. Launcher проверяет SHA256 debug EXE.
На системном D3D9 наблюдение `QueryInterface`/`Release`/`Reset`,
`CreateAdditionalSwapChain` и `GetSwapChain` устанавливается при первом создании
ресурса, после возврата из полной цепочки `CreateDevice`. Это сохраняет
обёртку Windows compatibility, если она установлена после нашего callback.
Последующая неизвестная замена метода по-прежнему не квалифицируется.

Проверка этой цепочки на собственном настоящем устройстве D3D9, включая
внешнюю обёртку, доступ к swap chain, переключение vertex processing и
окончательный `Release`:

```powershell
powershell -NoProfile -ExecutionPolicy Bypass -File tools/WinxRemix/Test-SystemSkinCapture.ps1 -Name packet-system-check
```

## Состояния материала и текстуры

После успешного исходного draw захват дополнительно пытается записать
`packet-NNNN-draw-NNNNNN.material.json`. Поле `materialWritten` в `draw_result`
отражает результат этой отдельной операции. Ошибка материала не уничтожает
уже сохранённый SKP1 и не меняет результат исходного draw.

Sidecar версии 1 связывает packet/frame/draw/shader с 24 render states,
состояниями восьми texture stages и samplers, матрицей TEX0 и копией texture0.
Пары состояний имеют вид `[D3D enum, DWORD value]`; float-состояния и матрица
сохранены как исходные DWORD-биты. Указатели в `renderer`, `nativeMaterial`
и `textureBindings` служат локальными идентификаторами происхождения и
не являются ресурсами для повторного использования в другом процессе.

До и после копирования перечитываются состояния устройства. Текстура должна
иметь подтверждённую историю создания, времени жизни и изменений содержимого
в native transport registry. Поддерживаются managed A8R8G8B8/X8R8G8B8 без usage,
не более 4096×4096 и 16 mip-уровней. Копирование всех уровней использует
READONLY lock, учитывает pitch и не сохраняет его padding. Проверка поколения
содержимого завершает операцию. Неизвестный alias или нарушение наблюдения
оставляет экспорт неподтверждённым.

Одинаковые поколения texture/content используют один `texture-NNNN.skt`.
После записи в текстуру требуется новый файл. Лимиты: 64 texture-файла,
8 МиБ на файл и 32 МиБ суммарно, 16 КиБ на material sidecar. Все файлы
создаются исключительно; ошибка записи останавливает затронутый экспорт.
`materialQualified:false` сохраняется даже при успешном захвате: снимок
состояний сам не устанавливает смысл shader COLOR0 и материал Remix.

Формат `SKT1` собственный, little-endian. Заголовок — семь uint32 (28 байт):
magic `0x31544B53`, version 1, полный размер, width, height, levels,
D3DFORMAT (21 — A8R8G8B8, 22 — X8R8G8B8). Каждый mip содержит четыре uint32:
width, height, rowBytes=`width*4`, dataBytes=`width*height*4`, затем BGRA-байты
плотных строк. Следующие размеры делятся на 2 с округлением вниз, минимум 1.
Хвостовые байты и дополнительные уровни после 1×1 запрещены. Alpha X8 семантически
равна 255 независимо от сохранённых неиспользуемых байтов.

Проверка файлов, mip-уровней и связей с фактическим shader selection:

```powershell
python tools/WinxRemix/analyze_skin_materials.py path/to/run --output path/to/fresh-report.json
```

Анализатор проверяет целостность захвата и явно отделяет отсутствующие
материалы. Он не исполняет шейдер, не сравнивает растр и не разрешает submit.

## Формат SKP1

Все числа little-endian. Заголовок — восемь `uint32`, 32 байта:

| Смещение | Значение |
| --- | --- |
| 0 | `0x31504B53`, байты `SKP1` |
| 4 | Версия 1 |
| 8 | Полный размер файла |
| 12 | Число вершин |
| 16 | Число индексов, кратное 3 |
| 20 | Число активных влияний 1–4 |
| 24 | Число матриц 1–16 |
| 28 | Attributes: bit0 — UV0; bit1 — исходный COLOR0 |

Далее идут palette: по 12 `float32` на матрицу, три строки shader-регистров;
вершины: по 80 байт; индексы triangle list: `uint32`.
Вершина содержит position[4], normal[4], weights[4] (`float32`), indices[4]
(`uint32`), UV[2] (`float32`), color ARGB (`uint32`), sourceIndex (`uint32`).
`sourceIndex` относится к исходному draw range до компактной перенумерации.
Неактивные lanes weights/indices равны 0. Отсутствующий UV равен 0,
отсутствующий color — `0xffffffff`.

Decoder проверяет полную длину, диапазоны и конечность чисел до публикации
результата. Частичный файл и хвостовые байты не принимаются. При ошибке
предыдущий выходной объект не изменяется.

## Проверка и replay

```powershell
powershell -NoProfile -ExecutionPolicy Bypass -File tools/WinxRemix/Test-SkinPackets.ps1 -Name packet-check -Platform x64
local-data/rtx-remix/skin-packet-tests/packet-check/test_skin_packets.exe --inspect path/to/packet.skp
local-data/rtx-remix/skin-packet-tests/packet-check/test_skin_packets.exe --deform-json path/to/packet.skp
```

`--inspect` выполняет decode и полную деформацию. `--deform-json` дополнительно
выводит по 8 чисел на вершину: position[4], normal[4]. Оба используют
[общую восстановленную формулу Fixed](../../../docs/engine/animation/skin-deformation.md).
Неопределённое направление нормали отклоняется. Это CPU replay, а не
проверка shader GPU или готовой интеграции Remix.

Проверка адаптера с моделируемыми native/transport данными:

```powershell
powershell -NoProfile -ExecutionPolicy Bypass -File tools/WinxRemix/Test-NativeSkinVertices.ps1 -Name packet-adapter-check
```

Исправление strides и ограничения Remix skin описаны в
[renderer-skinning-fix](../renderer-skinning-fix/README.md).
При выборе дальнейшего пути нужно сравнивать фактическую деформацию:
близкая к 1 сумма весов сама по себе не доказывает эквивалентность
явного последнего веса игры и неявного последнего веса Remix.

## Подготовка готовой геометрии

[winx_skin_packet_remix.h](../winx_skin_packet_remix.h) содержит собственную
подготовку данных для Remix без зависимости от SDK и без вызовов renderer.
`Bake` применяет общую формулу Fixed ко всем вершинам пакета и возвращает
`BakedMesh`: мировые позиции, единичные направления нормалей, UV, цвет,
исходные attribute flags и тот же uint32 triangle list. Используются все
записанные веса, включая последний, отрицательные и ненормированные.
Допуск определяется контрактом пакета; имена моделей не проверяются.

Направление нормали берётся из `xyz` общей float4-нормали и нормализуется
в трёх измерениях. Общая реализация и её промежуточная float4 не меняются.
Это подготовка геометрии, а не воспроизведение освещения оригинального shader.
Нулевая или неконечная нормаль отклоняется. Результат владеет своими массивами,
не удерживает native/COM-объекты и не меняется при последующей правке входа.
При ошибке прежний выход сохраняется.

`BakedVertex` — собственная структура, не двоичное представление
`remixapi_HardcodedVertex`. Поля API нужно заполнить явно. Для уже мировых
вершин consumer должен отключить skinning и задать единичное преобразование
instance: повторное применение костей или world transform меняет результат.
Загрузка через API, материал, камера и срок жизни renderer mesh остаются
отдельными условиями подключения consumer.

[winx_skin_packet_submit.h](../winx_skin_packet_submit.h) добавляет явную
границу `Prepare → Resource → Draw`. `Prepare` получает от политики материала
отдельный массив API vertex colors: исходный COLOR0 нельзя автоматически
считать albedo. Он по полям заполняет 64-байтные `remixapi_HardcodedVertex`,
обнуляет padding и сохраняет компактные индексы. Для cull CW каждый треугольник
разворачивается один раз; для NONE/CCW исходный порядок сохраняется.

`Resource` использует общий Surface cache с индексированным входом. Ключ
включает все вершины, индексы и material handle. Новая поза создаёт новый
immutable mesh; API UpdateMesh здесь нет. Skinning отключён, instance transform
единичный. Лимиты общего cache остаются 512 meshes, 64 МиБ геометрии и 256
материалов; память считается по компактным массивам. Материал должен уже
принадлежать этому cache, а освобождение меша предшествует его материалу.
В используемом Remix API повторная регистрация прежнего handle не обновляет
вершины, а новый handle не наследует идентичность предыдущей позы. Непрерывная
история движения требует отдельного решения для обновления геометрии или
GPU skinning.

`DrawResult` отдельно сообщает `apiCalled`, `apiSucceeded`, `stateStable`.
После `apiCalled=true` повторная отрисовка того же прохода как fallback
недопустима: ошибка API или retirement внутри callback не доказывает отсутствие
submit. Повторный вход отклоняется; отложенный retirement выполняется после
возврата внешнего вызова. Контекст камеры, актуальность native owner и
семантический допуск материала остаются обязанностью вызывающей стороны.
Этот backend ещё не подключает подавление игровых Skin draws.

Проверка этой полной операции с регистрирующей реализацией API, без GPU:

```powershell
powershell -NoProfile -ExecutionPolicy Bypass -File tools/WinxRemix/Test-SkinPacketSubmit.ps1 -Name fresh-submit-check
local-data/rtx-remix/skin-packet-submit-tests/fresh-submit-check/test_skin_packet_submit.exe --manifest paths.txt
```

Manifest содержит по одному пути к SKP1 на строку, максимум 128. Replay
передаёт все поля в записывающий API, проверяет identity/unskinned draw и
полное освобождение; цвета в нём диагностические. PASS не означает выполнения
bridge, renderer или квалификации материала.

`PrepareColor4` связывает эту подготовку с собственной начальной политикой
[winx_skin_material_plan.h](../winx_skin_material_plan.h). Для отдельно
подтверждённого Fixed ColorMode 4 выбираются reflectance=`MatDiffuse.rgb` и
emission=`authored COLOR0.rgb`. Старые `AmbientCol` и directional lighting
в материал не запекаются; их восстановленная формула остаётся эталоном
сравнения, а RT lights задаются отдельно. Превращение добавочного цвета в
emission — политика адаптера, не свидетельство свечения поверхности в игре.
Общий material backend использует roughness 0.5 как настройку прототипа;
это значение не восстановлено из исходного материала игры.

Для постоянного vertex RGB коэффициенты переносятся в отдельные albedo/emission
текстуры; API vertex RGB становится белым. Чёрный исходный RGB поэтому
обнуляет emission, сохраняя отражение. Если RGB меняется по вершинам, общий
множитель API подходит только при нулевом постоянном albedo. Остальные случаи
отклоняются без усреднения цветов. Используется общая факторизация
`material_channels::Factor`, не отдельная копия её математики.

Начальная политика требует исходных UV0/COLOR0, конечного MatDiffuse в [0,1]
и `MatDiffuse.a=1`; исходный vertex alpha в ColorMode 4 не используется.
Shader selection, отсутствие specular/pixel shader, умножение единственной
текстуры, sampler и alpha/blend state вызывающая сторона проверяет отдельно.
`ProjectColor4` и `PrepareColor4` сами не квалифицируют текущий игровой draw.
Коэффициенты предназначены для общего bounded DDS writer: он сохраняет mip
и alpha, tint RGB округляет в UNORM8. Загруженная текстура в Remix заменяет
material constant, поэтому tint должен попасть в texture payload.

### Текущий пакет и подготовка состояния

[winx_native_skin_packet_source.h](../winx_native_skin_packet_source.h)
выделяет получение текущего пакета из sampled захвата. После установки
наблюдателей `native_skin_source::Scope` сохраняет успешное наблюдение на время
одного исходного Skin call. `CopyDraw` связывает его с текущим mesh submission
и точным диапазоном D3D draw. Частота кадров, журнал и лимиты файлов не участвуют
в сборке пакета; захват использует тот же общий сборщик.

Результат содержит собственные вершины, индексы, палитру и `Stamp` без
заимствованных указателей на массивы. Адреса native/COM объектов в нём —
только признаки идентичности, не владение объектами. `Current` заново получает
исходники под общей блокировкой, сверяет scope, заголовки, палитру, upload,
declaration и поколения ресурсов. Хеши исходных VB/IB дополнительно обнаруживают
изменение байтов при повторном использовании номера поколения. Проверка
действует внутри исходного вызова, не доказывает долговременную идентичность.
Сам `CopyDraw` не читает фактически привязанные D3D buffers/shaders/constants;
их соответствие и неизменность после внешних вызовов проверяются отдельно.

[winx_skin_draw_state.h](../winx_skin_draw_state.h) предоставляет общий
`Snapshot` и `ReadSnapshot` для захвата и подготовки. Временные COM references
освобождаются до возврата; ошибочное чтение сохраняет предыдущий результат.
Снимок не содержит shader constants и сам по себе не защищает от изменения
состояния во время внешнего вызова.

[winx_skin_draw_plan.h](../winx_skin_draw_plan.h) соединяет этот снимок с
`skin_packet_submit::PrepareColor4`. Входной shader и смысл переданного
`MatDiffuse` должны быть доказаны вызывающей стороной. Начальный контракт:

- Depth test/write включены, сравнение `LESSEQUAL`; blending — `ONE/ZERO/ADD`,
  отдельный alpha blend выключен, запись RGBA полная.
- Поддерживаются три cull mode и общая таблица alpha compare; cull задаёт
  порядок индексов и `doubleSided`, alpha state переносится в instance.
- Единственная текстура использует `MODULATE(TEXTURE,CURRENT)` для RGB/alpha,
  UV0 без преобразования, repeat и linear min/mag/mip без LOD bias и sRGB.
- Pixel shader, дополнительные текстуры, fog, stencil, scissor и specular
  отклоняются. `D3DRS_LIGHTING` не трактуется как свет для уже доказанного VS.

Результат — геометрия, albedo/emission plan, sampler, texture equation и
instance state. Ошибка содержит причину и сохраняет предыдущий результат.
Имена моделей и уровней не участвуют в допуске. Эта подготовка не создаёт
ресурсы и не подменяет игровой draw: проверка shader bytes/register layout,
текущей камеры, света и texture ownership остаётся отдельным этапом.

CPU-проверка этой политики с параметрами FLP1:

```powershell
local-data/rtx-remix/skin-packet-submit-tests/fresh-submit-check/test_skin_packet_submit.exe --project-color4 input.skp parameters.flp
```

Общий API consumer проверяется и через настоящий serializer/owning decoder
Mesh, Instance и Blend в обе стороны x86/x64:

```powershell
powershell -NoProfile -ExecutionPolicy Bypass -File tools/WinxRemix/skinning-bridge/Test-BakedPackets.ps1 -Name fresh-wire-check -Manifest paths.txt
```

Это обмен файлами с точной сверкой полей и байтов; IPC и renderer не запускаются.
Игровое наблюдение остаётся только x86, owned packet/replay/backend доступны
на обеих архитектурах.

`InspectRemixWeights` отдельно проверяет весовую семантику закреплённого
Remix: вычитает первые B−1 весов последовательно в float32 и сравнивает
остаток с записанным последним весом. `WeightCompatibility::Exact()` требует
точного равенства, отсутствия отрицательных весов и конечного остатка.
Метод не исправляет веса. Это консервативная проверка только весов;
она не доказывает побитовое совпадение GPU, выбранного shader или материала.
Даже маленькая разница не превращается автоматически в разрешение submit.

Полное CPU/GPU-сравнение сохранённых пакетов и контроль передачи готовых
вершин описаны в [инструкции GPU-стенда](../renderer-skinning-fix/README.md#сравнение-сохранённых-пакетов).

## Общая проверка directional/color

[Test-FixedLighting.ps1](../Test-FixedLighting.ps1) проверяет общую
[directional/color-формулу Fixed](../../../docs/engine/materials/shader-contract.md)
и собирает небольшой CPU replay:

```powershell
powershell -NoProfile -ExecutionPolicy Bypass -File tools/WinxRemix/Test-FixedLighting.ps1 -Name fresh-lighting-check -Platform x64
local-data/rtx-remix/fixed-lighting-tests/fresh-lighting-check/test_fixed_lighting.exe --packet input.skp parameters.flp output.bin
```

Файл параметров `FLP1` — собственный вход стенда, 368 байт little-endian.
Четыре uint32: magic `0x31504C46`, version 1, ColorMode 0–7, directional count
0–8. Далее float32: три float4-регистра view, AmbientCol, MatDiffuse,
ConstColor; восемь пар float4 LightDir/LightMatDiff. Регистры извлекаются по
CTAB конкретного shader; число и тип света проверяются отдельно по его
специализации. Этот формат не поддерживает point, spot и specular.

Выход содержит семь float32 на вершину: view normal[3], raw color[4].
Тест использует общий SKP1 decoder и общую Fixed-деформацию; запись полностью
завершается до сообщения PASS. Направление и цвет до упаковки — разные
результаты: совпадение packed ARGB после насыщения не доказывает совпадение
исходного float-цвета или нормали. Этот CPU replay сам не исполняет bytecode.
