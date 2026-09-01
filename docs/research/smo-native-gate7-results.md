# Gate 7: end-to-end release matrix и stress

Дата проверки: 29 августа 2026 года.

Статус: **пройден**. Три уровня разных размеров, два skinned-персонажа,
контрольные pristine-запуски и негативные ветви memory/timeout/cancellation
прошли обязательную PC release-матрицу. Все семь MVP-шагов закрыты.

## Покрытие редакторских операций

Каждый stress-run начинается гарантированным prelude, поэтому случайное
распределение не может пропустить обязательный класс операции:

- transform существующего visual placement;
- shared placement без копирования физического mesh-ресурса;
- внешний rigid import и полная замена модели;
- замена поддерживаемой fixed-size texture;
- удаление placement;
- add/transform/delete collision branch;
- два archive/build/reopen checkpoint;
- финальные build, повторный build и re-import/build.

На каждом checkpoint текущий документ проходит строгий parser без errors.
Финальный writer повторно проверяет layout, inline sizes, object references и
отношения проекта. Три финальных файла для каждого run — обычная сборка,
повторная сборка и сборка после re-import — совпали побайтно и по SHA-256.

## Stress matrix

| Уровень | Seed / правки | Объекты source → output | Размер output | Build / весь run | Peak working set | SHA-256 |
|---|---:|---:|---:|---:|---:|---|
| `Gardenia01` | `82901` / 100 | 5 936 → 6 014 | 7 262 864 | 4,060 / 190,249 s | 338,9 MiB | `2979BA74D903040FB3CB90859F98FE00CACB911B9599BFC4483BE2E546EE613C` |
| `Alfea02_old` | `82744` / 1 000 | 4 266 → 4 781 | 7 190 186 | 17,051 / 2 120,068 s | 356,9 MiB | `2E95388BC62E29A29F7ADE974DF9C09EEE2AE12283E6185648FC59CB943B4884` |
| `Domino04` | `82904` / 100 | 3 693 → 3 749 | 11 051 254 | 2,014 / 44,360 s | 488,7 MiB | `2B98DE9C276619CD53DD24A2CF2B4A4576F123B5C126FAA2881DC05596E3E9FE` |

Распределение 1 000 операций Alfea: 602 transforms, 238 reference placements,
1 external import, 1 complete replacement, 53 texture replacements, 67 deletes
и 38 collision operations. Два archive checkpoint также успешно собраны и
повторно открыты; журнал занял 156 566 байт, Undo depth ограничен 256.

Отчёты находятся в
`local-data/validation-results/mvp-gate7-release-20260829`.

## Исправленный texture boundary

Первый Gardenia-run обнаружил две связанные ошибки:

1. project texture replacement сопоставлял запись `TextureData` по порядковому
   номеру, хотя SMOTextureTool адресует физический блок по file offset;
2. stress/UI-каталог считал изменяемым любой объект класса `TextureData`, хотя
   часть встречающихся payload не является поддерживаемым fixed-size slot.

`SmoProjectTextureReplacement` теперь сопоставляет блок по `BlockOffset ==
PhysicalOffset` и предоставляет каталог только фактически записываемых texture
object IDs. Неизвестные варианты остаются read-only и byte-preserved. Повторный
запуск с тем же seed прошёл 100/100 операций.

## Native release matrix

Manifest:
[`mvp-gate7-release.json`](../../tools/SmoViewer/SmoNativeValidator.Cli/manifests/mvp-gate7-release.json).

Run:
`local-data/validation-results/mvp-gate7-release-native-20260829/run-20260829-190639-801`.

| Кейс | Scene-ready | Время | Native результат |
|---|---|---:|---|
| pristine Gardenia до правок | passed | 33,362 s | level 1, pending 0, non-null resource |
| Gardenia после mixed stress | passed | 25,742 s | level 1, pending 0, non-null resource |
| Alfea после 1 000 правок | passed | 24,819 s | level 28, pending 0, non-null resource |
| крупный Domino после mixed stress | passed | 23,619 s | level 7, pending 0, non-null resource |
| Bloom skinned import | passed | 25,972 s | level 2, `CP08`, non-null resource |
| Flora skinned import | passed | 27,865 s | level 10, non-null resource |
| pristine Alfea после edited assets | passed | 22,603 s | level 28, pending 0, non-null resource |

Итого: 7/7, serializer version `0x26`, `crash=none/none`. Каждый кейс использовал
собственный launch workspace; executable и pristine Media не изменялись.

При загрузке `Domino04` движок сообщил об одной degenerate collision face. Это
не structural/native failure: SMO вернул ненулевой resource, активировал level 7
и пережил окно наблюдения. Предупреждение относится к качеству сгенерированной
collision-геометрии и остаётся диагностикой конкретного stress-кандидата.

## Gameplay evidence

Отдельный manifest
[`mvp-gate7-release-gameplay.json`](../../tools/SmoViewer/SmoNativeValidator.Cli/manifests/mvp-gate7-release-gameplay.json)
запускает точный Alfea SHA после 1 000 операций. После `SCENE01` helper послал
DirectInput `DIK_W=0x11` на 6 секунд и relative mouse input. Кадры подтверждают
переход из витринной зоны в коридор и изменение направления камеры; native run
завершился `Passed` за 48,817 s без crash.

Evidence:
`local-data/validation-results/mvp-gate7-release-gameplay-20260829`.

## Memory, timeout, cancellation и partial output

| Негативная ветвь | Результат |
|---|---|
| Private-memory watchdog | Лимит 64 MiB; child остановлен через 3,502 s при 69 365 760 bytes. Остался только failure report. |
| Timeout watchdog | Лимит 1 s; child остановлен через 1,073 s. Остался только failure report. |
| Cooperative cancellation | Автотест отменяет реальную save pipeline на `fish.smo` и `Alfea02_old.smo` до install. Существующий output остаётся побайтово прежним; `.tmp` и `.bak` отсутствуют. |
| Writer verification failure | Gate 1 regression оставляет существующий output прежним и очищает same-directory temporary file. |

У stress runner появились воспроизводимые optional timeout seconds рядом с
output root и memory limit. Оба supervisor failure report сохраняют фактическую
границу, peak private bytes и elapsed time.

## Автоматические проверки

- `SmoLVLcreator.CoreTests` + реальный `Alfea02_old.smo`: 1 872 assertions;
- `SmoImporter.FormatTests --atomic-output-regression`: 11 assertions;
- `SmoNativeValidator.Tests`: 299 assertions;
- `SmoLVLcreator.Gui`: 0 warnings, 0 errors;
- `SmoImporter.Gui`: 0 warnings, 0 errors.

CoreTests build отдельно сообщил только `NU1900`: в изолированной среде был
недоступен сетевой NuGet vulnerability audit. Компиляционных предупреждений и
ошибок кода нет.

## Итоговая граница

PC MVP для запуска SmoLVLcreator и импорта rigid/skinned-моделей закрыт.
Неизвестные payload не получают выдуманной семантики: они сохраняются байт-в-байт
либо остаются read-only. Расширенный runtime reverse engineering, полный набор
material/animation/navigation authoring и PS2 writer остаются post-MVP и не
блокируют начало пользовательского тестирования редактора.
