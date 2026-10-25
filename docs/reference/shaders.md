# Каталог шейдеров

Полный список исследованной внешней PC-папки `Shaders`: **9 файлов, 11 стадий** (6 vertex и 5 pixel). Восстановленное HLSL-представление находится в **7 исходных файлах**. Стадия, семейство и сгенерированный вариант учитываются отдельно.

[Машиночитаемый реестр](shaders.json) · [Карточки](shaders/README.md) · [Исходники](../../Sparkplug/Shaders/README.md) · [Исходный shader-контракт](../engine/materials/shader-contract.md)

## Программы

| ID | Ресурс / запись | Стадия | Назначение | Карточка | Восстановленный код |
| --- | --- | --- | --- | --- | --- |
| `pc.ballistic.vs` | `BallisticPFX.vsh` | vertex | Движение, цвет и размер точечных частиц | [BallisticPFX](shaders/ballistic-pfx.md) | [BallisticVS](../../Sparkplug/Shaders/PC/BallisticPFX.hlsl) |
| `pc.basic.vs` | `BasicShader.vsh` | vertex | Преобразование позиции четырьмя загруженными регистрами | [BasicShader](shaders/basic.md) | [BasicVS](../../Sparkplug/Shaders/PC/Basic.hlsl) |
| `pc.blur.vs` | `Blur.vsh` | vertex | Экранный quad и четыре смещённых UV | [Blur](shaders/blur.md) | [BlurVS](../../Sparkplug/Shaders/PC/Blur.hlsl) |
| `pc.blur.ps` | `Blur.psh` | pixel | Среднее четырёх выборок RGBA | [Blur](shaders/blur.md) | [BlurPS](../../Sparkplug/Shaders/PC/Blur.hlsl) |
| `pc.blur-luminance.ps` | `BlurLuminance.psh` | pixel | Размытие с умножением RGB на яркость | [BlurLuminance](shaders/blur.md) | [BlurLuminancePS](../../Sparkplug/Shaders/PC/Blur.hlsl) |
| `pc.bumpmap.ps` | `Bumpmap.rfx:0` | pixel | Normal map, diffuse и specular восьмой степени | [Bumpmap](shaders/bumpmap.md) | [BumpmapPS](../../Sparkplug/Shaders/PC/Bumpmap.hlsl) |
| `pc.bumpmap.vs` | `Bumpmap.rfx:1` | vertex | Кодирование tangent-space направлений света и half-vector | [Bumpmap](shaders/bumpmap.md) | [BumpmapVS](../../Sparkplug/Shaders/PC/Bumpmap.hlsl) |
| `pc.fixed.vs` | `Fixed.rfx:0` | vertex | Skinning, выбранное освещение, ColorMode и UV | [Fixed](shaders/fixed.md) | [FixedVS](../../Sparkplug/Shaders/PC/Fixed.hlsl) |
| `pc.fixed.ps` | `Fixed.rfx:1` | pixel | Возврат интерполированного COLOR0 без texture samples | [Fixed](shaders/fixed.md) | [FixedPS](../../Sparkplug/Shaders/PC/Fixed.hlsl) |
| `pc.luminance.ps` | `Luminance.psh` | pixel | Умножение RGB на шестнадцатую степень яркости | [Luminance](shaders/luminance.md) | [LuminancePS](../../Sparkplug/Shaders/PC/Luminance.hlsl) |
| `pc.shadow-volume-point.vs` | `ShadowVolumePoint.vsh` | vertex | Радиальное вытягивание теневого объёма точечного света | [ShadowVolumePoint](shaders/shadow-volume-point.md) | [ShadowVolumePointVS](../../Sparkplug/Shaders/PC/ShadowVolumePoint.hlsl) |

## Программы внутри executable

PS2-образы учитываются отдельно от PC vertex/pixel-стадий. Границы DMA/VIF-контейнера и payload установлены; семантическое восстановление инструкций и аппаратное исполнение остаются открытыми.

| ID | ELF VA контейнера | VU payload, байт | Instruction pairs | MPG-блоки | Описание |
| --- | --- | ---: | ---: | ---: | --- |
| `ps2.vu.0043a600` | `0x0043A600` | 10464 | 1308 | 6 | [Контракт](shaders/ps2-vu.md) |
| `ps2.vu.0043cf20` | `0x0043CF20` | 12408 | 1551 | 7 | [Контракт](shaders/ps2-vu.md) |
| `ps2.vu.0043ffe0` | `0x0043FFE0` | 12520 | 1565 | 7 | [Контракт](shaders/ps2-vu.md) |
| `ps2.vu.00443110` | `0x00443110` | 1272 | 159 | 1 | [Контракт](shaders/ps2-vu.md) |

Единица VU для этих путей не квалифицирована. Образ, MPG-блок, callback и entry point не считаются одной и той же единицей покрытия. Полные извлечённые bytes/listings остаются локальными данными.

[Поиск в executable и условия полноты](../engine/materials/shader-discovery.md).

## Что означает восстановление

Каждая запись имеет читаемое семантическое HLSL-представление. Исходные assembly 1.1 выражены через именованные вычисления; текстовые RFX-шаблоны реконструированы с явными параметрами. Это не заявление о восстановлении оригинальных имён, побитовом совпадении инструкций, всех GPU-пограничных значений или готовности адаптера Remix.

Исходные стадии имеют shader model 1.1. Профили 3.0 в реестре служат для проверки читаемой реконструкции; они не меняют версию оригинала и не означают установку этих программ в игру.

## Генерация и код EXE

`Fixed.rfx` порождает варианты по числу весов, ColorMode, упорядоченному свету, specular, числу UV-выходов и UV-маске. Это одна параметризованная стадия, а не отдельная запись на каждую комбинацию. Нулевой вес в штатном manager обходит создание такого VS.

EXE содержит [выбор вариантов](../engine/materials/shader-generation.md), [сборку/компиляцию](../engine/materials/shader-compile.md) и [подготовку констант](../engine/materials/shader-constants.md). Они учитываются отдельно от GPU-программ. Наличие SDK assembler/compiler не доказывает наличия встроенного shader payload.

## Границы полного списка

Перечень полон для исследованной внешней PC-папки, включая обе стадии каждого RFX. Из PS2 ELF выделены указанные выше upload-образы. Исчерпывающая инвентаризация всех runtime-only источников PC/PS2, версий игры и дополнительных пакетов остаётся открытой. Создание программы при старте не доказывает её использование при draw; отдельный наблюдавшийся набор не доказывает покрытие всех уровней.

Наличие HLSL, успешная компиляция, численное сравнение и проверка настоящей игры — разные уровни проверки. Реестр учитывает исходники и контракты; текущие игровые результаты и происхождение проверок хранятся отдельно.
