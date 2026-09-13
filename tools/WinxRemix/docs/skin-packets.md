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
На системном D3D9 наблюдение `QueryInterface`/`Release`/`Reset` устанавливается при первом создании
ресурса, после возврата из полной цепочки `CreateDevice`. Это сохраняет
обёртку Windows compatibility, если она установлена после нашего callback.
Последующая неизвестная замена метода по-прежнему не квалифицируется.

Проверка этой цепочки на собственном настоящем устройстве D3D9, включая
внешнюю обёртку, переключение vertex processing и окончательный `Release`:

```powershell
powershell -NoProfile -ExecutionPolicy Bypass -File tools/WinxRemix/Test-SystemSkinCapture.ps1 -Name packet-system-check
```

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
