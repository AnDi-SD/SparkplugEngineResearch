# Material power: BloomX/Icy и unlit Submit

Сохранённые `loaded-materials/*-bridge-final.json` от9 сентября подтверждают:

| Файл | Selected material ID / physical index | power_initialized | power |
|---|---|---:|---:|
| BloomX | 7/6, 90/89, 25/24, 29/28, 109/108, 113/112, 117/116 | 1 у всех7 | 0 |
| Icy | 5/4 | 1 | 100 |
| Icy | 73/72, 77/76 | 1 | 0 |

Во всех10 raw material state8=4. Это lit материалы; их нельзя объявлять unlit2
ради обхода. Power guard не блокирует эти inputs. Hashes обоих pristine SMO
сейчас повторно совпали с captures; нового loader/corpus/guest run не было.
Capture DLL — `11D125C08DA8528EB2BC749B56BAEFA9EB81F3C319313EB439BCD5440B468A62`;
результаты не выдаются за новую проверку нынешней DLL. Shared material field2
читает20 bytes и вызывает существующий power setter, сохраняя initialized flag.

`4BE19A` копирует17 words material+78→renderer+E4A4, включая неизвестный power.
Для mode2 `4BDB1A..4BDB2D` пропускает чтение E4E4: оно нужно только modes3/4/5.
Однако `4BC5C5` передаёт весь block внешнему SetMaterial, а automatic shader key
в `4BE3F5` безусловно вызывает getter `435500` (material+B8). Сравнение
`4BE3F8` и запись bit24 в `4BE405` не зависят от lighting mode.

No-weight manager `4C89A9→4C8DD7` возвращает NULL до cache lookup, но это не
общая возможность объявить power=0: копирование/device payload уже состоялись.
Новую validity/opaque ветвь для этого неиспользуемого случая не открываем,
guard сохраняем. При отсутствующем field2 конкретный недостающий input —
доказанная запись power через setter `4354F0`, а не новое default значение.
Unknown power renderer fallback C9C0 не мешает Submit, когда этот объект
используется только как источник двух fallback texture layers.

Следующий конкретный вход для этих lit материалов — actual selected light list
`LightSubmissionForAnalysis::input`, полученный из RenderNode light cacheF0.
Общие producer/consumer уже проверены в [CP93](native-pc-skin-selected-light.md),
но текущий Viewer shader всё ещё использует собственное фиксированное освещение
(`SmoGpuSceneRenderer.cs:751`). Следующий шаг — установить источник этого cache
для одного реального Viewer draw; нельзя молча подставить empty/NULL list.
Это граница подключения, не утверждение об отсутствующем игровом алгоритме.

Локальный компактный audit с hashes и адресными original bytes:
`local-data/results/tools-core-cycle-20260910-0730/material-preview/specular-power/audit.json`.
Исходники и интерфейсы этим аудитом не менялись.
