# Текущий Skin draw и shader-контракт

`-SkinDrawAudit` проверяет полный текущий кандидат перед исходным indexed draw.
Геометрия берётся из [общего источника пакетов Skin](skin-packets.md), независимо
от записи файлов и частоты диагностических выборок. Наблюдение сохраняет
исходный draw и само не вызывает Remix API.

## Подготовка программ

Сборка исходника использует общую восстановленную
[подготовку PC shader](../../../Sparkplug/Code/SparkplugPC/spPCShaderSource.h).
Собственный helper не содержит второй копии правил shader manager.
Для подготовки каталога нужны MSVC x64, Python x64, оригинальный локальный
`Fixed.rfx` и D3DX9 SDK24 соответствующей архитектуры.

```powershell
powershell -NoProfile -ExecutionPolicy Bypass -File tools/WinxRemix/Build-ShaderSource.ps1 -Name shader-source-check
python tools/WinxRemix/Prepare-SkinShaders.py --source-helper local-data/rtx-remix/shader-source-build/shader-source-check/shader_source.exe --fixed local-data/pc-pristine/Shaders/Fixed.rfx --output local-data/rtx-remix/skin-shader-contracts/skin-contract-check
```

Генератор принимает установленную реализацию Fixed и создаёт 16 вариантов:
B1–B4, ColorMode4, UV0 без texture transform, 0–3 directional lights,
без specular. Ограничение относится к программе, не к модели или уровню.
Другой исходный shader требует отдельного установления семантики.
Игровой исходник и скомпилированные программы остаются в `local-data/`.

Каталог WSF1: little-endian uint32 magic `0x31465357`, версия 1, число записей,
32 байта SHA256 исходной реализации; далее для каждой записи два слова
shader key, uint32 длина bytecode и сами байты. Каталог ограничен 1 МиБ,
64 записями, каждая программа — 16 КиБ. Дубли ключей и хвост не допускаются.
Это доверенный локально подготовленный вход, а не цифровая подпись.

При draw сравниваются native selection, фактически связанный COM shader,
его байты, инструкции VS1.1 и все отражённые параметры CTAB: имена,
регистры, типы, размеры и массивы. Различия creator/comment padding не
меняют семантику. Изменение инструкции или описания константы отвергается.

## Проверка состояния

Проверяются VB/IB/declaration и диапазон draw, текущая палитра в shader
константах, native matrix caches, camera apply, завершённый world update,
MatDiffuse, Color4 material policy, состояния sampler/stages и поколение
текстуры. Первая поддержанная ветвь требует единичную native world matrix;
палитра уже задаёт мировую геометрию. Неизвестная ветвь нормали не угадывается.

Общий guard и перехваты setter отслеживают изменения состояния, включая
все шесть типов VS/PS-констант и StateBlock.Apply. Ошибочный setter тоже
инвалидирует чтение. Getter может временно вызвать нефинальный Device.Release;
такой вызов блокирует повторное чтение на время выполнения, но сам не
меняет состояние рендера. Вложенный setter и финальный Release инвалидируют
его по-прежнему. COM references освобождаются до заключительной проверки.

`cameraSubmitted` отдельно подтверждает принятие текущей камеры клиентским
API. `directLightsSubmitted` требует успешной отправки всей текущей группы
прямых источников, сохранения native registry/payload, API-владельцев и
политики исключения дублирующего legacy light. Ambient/environment сюда
не входят. Смешение отправок разных сцен в одном кадре не квалифицируется.
Эти поля не доказывают завершение GPU-команды.

## Запуск и проверки

К существующим параметрам `Start-Probe.ps1` добавляются `-SkinDrawAudit`
и `-SkinShaderContracts <путь к contracts.wsf>`. Launcher копирует каталог
в новый run и записывает его SHA256. Для системного D3D9 доступны
`-DebugMenu -Backend system`; API-поля в этом режиме остаются ложными.
Журнал `skin-draw-source.jsonl` ограничен 16 МиБ. Число вершин в повторных
выборках не является числом уникальных вершин сцены.

```powershell
powershell -NoProfile -ExecutionPolicy Bypass -File tools/WinxRemix/Test-SkinShaderContract.ps1 -Name shader-contract-check -Platform x86 -Catalog local-data/rtx-remix/skin-shader-contracts/skin-contract-check/contracts.wsf
powershell -NoProfile -ExecutionPolicy Bypass -File tools/WinxRemix/Test-SystemSkinCapture.ps1 -Name skin-device-check
```

Собственные CPU/COM fixtures проверяют отдельные отказы, повторный вход и
владение; игровой запуск подтверждает связь с фактическими исходными draw.
Совместимость [постоянной геометрии с весами Fixed](../renderer-skinning-fix/README.md)
и окончательный live submit остаются отдельными операциями.

## Постоянная геометрия и изменяемая палитра

[Адаптер signed skinning](../winx_skin_gpu_submit.h) передаёт исходные позиции,
нормали, UV, явно подготовленные цвета и постоянные массивы весов/индексов.
Палитра передаётся отдельным расширением instance. Её изменение между позами
не пересоздаёт mesh. Разные экземпляры персонажей требуют разных устойчивых
идентификаторов владельца; одинаковый адрес после уничтожения объекта нельзя
считать прежним экземпляром.

Общий владелец Surface учитывает память весов и индексов, хранит необходимый
размер палитры по созданной геометрии и уничтожает mesh до его material.
Перед единственным вызовом DrawInstance повторно проверяется актуальность
состояния вызывающей стороны. Ошибка после начала API-вызова не разрешает
повторить исходный draw: неизвестно, успела ли команда попасть в очередь.

`Test-SkinPacketSubmit.ps1` проверяет обе формы геометрии, смену поз без
пересоздания mesh, разделение владельцев, отказ при короткой палитре,
повторный вход и освобождение при частично успешном Create/Draw.
Это проверка API-контракта с записывающими callbacks. Автоматическое получение
устойчивого native owner и подключение к игровому draw пока не реализованы.
