# Нативная проверка SMO через WinxClub.exe

## Исправление ложных crash

Первые сеансы `SmoNativeValidator` ошибочно классифицировали все модели как crash.
32-битная игра под 64-битным debugger сообщает software breakpoint и single-step
кодами WOW64 `0x4000001F` и `0x4000001E`, тогда как harness распознавал только
обычные `0x80000003` и `0x80000004`. В результате собственный `INT3` валидатора
передавался игре и становился second-chance exception. Оба семейства кодов теперь
обрабатываются одинаково.

## Совместимость executable

SHA-256 и размер `WinxClub.exe` больше не используются как условие допуска.
В executable PE-секциях ищутся masked x86-сигнатуры `BuildAssetPath`,
`ResourceLoad` и семантическая цепочка FFPS magic/version. Адреса хранятся как
RVA и перед установкой `INT3` повторно проверяются по маске уже в памяти дочернего
процесса. Отсутствующая или неоднозначная обязательная сигнатура означает
`InstrumentationUnavailable`, а не ошибку модели или несовпадение хеша.

## Контрольный прогон 10 моделей

Прогон выполнен 2026-08-14 на `local-data/pc-pristine/WinxClub.exe`. Все модели
подменяли один logical slot `Characters\Bloom\bloom_jeans.smo`; на кейс создавался
новый принадлежащий validator процесс игры. Результат: **9 Passed, 1 Crash**.

| Модель | Результат |
|---|---|
| `bloom_jeans_from_uzhs.smo` | Passed |
| `bloom_jeans_from_uzhs_crash.smo` | Crash, ожидаемый контроль |
| pristine `bloom_jeans.smo` | Passed |
| pristine `bloom_ball.smo` | Passed |
| pristine `bloom_ballroom.smo` | Passed |
| pristine `bloom_bike.smo` | Passed |
| pristine `bloom_business.smo` | Passed |
| pristine `bloom_crystal.smo` | Passed |
| pristine `bloom_goth.smo` | Passed |
| pristine `bloom_hippie.smo` | Passed |

У падающего контрольного файла путь был подменён на 41,56 секунде. Файл прошёл
`FFPS01`, `FFPS02` и `FFPS03`, после чего основной loader thread получил
second-chance `0xC0000005` по адресу `0x004ABCFB`. Последняя целевая точка —
`FFPS03`; crash происходит после принятия контейнера и версии, а не в заголовке
FFPS.

Дизассемблирование и сопоставление good/bad-файлов локализовали точную причину.
Для texture formats `0x32E3`/`0x43E3` байт `spTextureData + 0x3C` является
обязательным marker `00` сериализатора, а BGRA-пиксели начинаются только с
`+0x3D`. Прежний full-RGBA writer ошибочно считал marker первым Alpha-байтом и
записал туда `FF`. Native loader прочитал высоту/число строк как `0xFF000100`
вместо `0x00000100`; затем virtual method `spDXTexture` вышел за 256 строк
буфера и получил access violation на `rep movsd` в `0x004ABCFB`.

Это опровергает прежнюю гипотезу о том, что игра в принципе запрещает замену
Alpha. Причиной контрольного crash был off-by-one нашего parser/writer. Вывод
подтверждён всем pristine-корпусом: marker равен нулю у 2348 из 2348 блоков
`0x32E3`/`0x43E3`.

## Проверка исправленного полного BGRA/Alpha

Тем же диагностическим путём создан исправленный вариант
`local-data/native-validation-corrected-rgba/02_texture_rgba.smo`: geometry и
object graph взяты из рабочего `bloom_jeans_from_uzhs.smo`, а donor Alpha записан
в настоящий BGRA payload с `+0x3D`. Оба marker на `+0x3C` остались `00`; новый
Viewer parser выполнил 317 проверок.

Нативный запуск завершился `Passed`: целевой путь перенаправлен, пройдены
`FFPS01`/`FFPS02`/`FFPS03`, `ResourceLoad` вернул ненулевой объект, достигнут
`CP08`, процесс пережил окно наблюдения без исключения. Артефакты:
`local-data/native-validation-corrected-rgba-runs/run-20260814-133345-321/`.
Таким образом, исправленный controlled case подтверждает совместимость полной
BGRA/Alpha-записи на уровне native load. Это ещё не общая визуальная проверка
donor Alpha на всех материалах, поэтому production importer пока сохраняет Alpha
target по консервативной политике.

Полные артефакты локального прогона:
`local-data/native-validation-runs/run-20260814-125205-382/` (`summary.json`,
`summary.tsv` и десять отдельных JSONL).

После введения per-thread стека вложенных `ResourceLoad` контрольная пара была
повторена на финальной логике: исправный файл снова получил `Passed`, падающий —
тот же `0xC0000005` в `0x004ABCFB` после `FFPS03`. Артефакты повторной проверки:
`local-data/native-validation-post-stack/run-20260814-131200-366/`.

`Passed` в текущем MVP означает: целевой путь действительно перенаправлен, native
loader прошёл target-scoped FFPS magic/version, вернул ненулевой ресурс, а процесс
пережил заданное окно наблюдения. Это ещё не полный gameplay- или per-object trace.

## Скрытый запуск и debug menu

У игры не найдено игровых аргументов командной строки для загрузки уровня или
произвольного SMO. Строки `-install`/`-remove` относятся к SecuROM, а собственный
CLI есть только у внешнего Tweak Center. Нативная игра читает скрытые параметры
из `winx.ini`:

- `fullScreen=false` включает штатный оконный режим без EXE-патча;
- `startLevel=N` передаёт 1-based ID непосредственно менеджеру смены уровня;
- `testCinematic`/`cinematicToTest` запускают cinematic path, но не дают
  произвольный resource load;
- `loadFromCD`, `buildPCK`, `loadFromPCK`, `firstPCKLevel` и `enableDialog`
  относятся к другим режимам resource/runtime startup.

Debug-menu patch устанавливает F1-hook и открывает штатное меню. Навигация —
стрелки и Enter; команда `LOAD LEVEL` выбирает только встроенный числовой уровень
и не принимает путь SMO. Поэтому validator не зависит от debug-patched EXE:
`startLevel` быстрее и воспроизводимее, а точки загрузчика находятся по коду.

Карта `startLevel` восстановлена из таблицы игры: 1–37 — основные уровни,
41–49 — star/race/battle challenges. Слоты 38–40 имеют пустой SPL descriptor,
50 (`Gardenia04`) не имеет level SMO. Контрольный `startLevel=2` действительно
дал переход состояний `0 -> 54 -> 2 -> 71` и запросил
`Characters\Bloom\bloom_jeans.smo`; Media продолжал разрешаться через штатный
registry `MediaPath`, хотя рабочий каталог был временным.

## Быстрый универсальный маршрут

Для ежедневной проверки выбран ранний гарантированный slot
`Menus\mousecursor.smo`. Validator сохраняет исходный игровой путь модели как
метаданные, но staging и сравнение `BuildAssetPath` выполняет по фактическому
trigger. Тем самым игра начинает разбирать выбранный SMO ещё при startup и не
требует ручного прохождения до сцены.

Повторный пакет из десяти Bloom-моделей завершился за 38,492 секунды: девять
рабочих файлов получили `Passed`, а заведомо повреждённый — тот же second-chance
`0xC0000005` в `0x004ABCFB`. Прежний контекстный пакет занимал 448,728 секунды,
то есть ранний slot оказался примерно в 11,7 раза быстрее. Отдельный `Troll.smo`
также получил `Passed` через ранний trigger.

Артефакты: `local-data/native-faststart-probes/early-batch-results/` и
`local-data/native-faststart-probes/results/`. Они локальные и не входят в Git.

Граница результата принципиальна: быстрый `Passed` подтверждает нативный
`BuildAssetPath`, `ResourceLoad`, target-scoped FFPS magic/version, создание
ресурса и окно стабильности. Он не подтверждает поведение модели в конкретном
уровне, SPT/SPL-скрипты или анимации. Для таких случаев остаётся контекстный
маршрут с исходным logical path и опциональным `startLevel`.

## Финальная интеграционная проверка

После переноса маршрутов в `SmoNativeValidator.Core`, Viewer и CLI повторён тот
же пакет из десяти моделей уже через публичный `NativeValidationRequest` и
изолированный launch workspace. Итог: **9 Passed, 1 ожидаемый Crash** за 48,68 с
при трёхсекундном окне наблюдения. Контрольный crash получил типизированную фазу
`DuringTargetLoad` и уверенность `Direct`; адрес и код прежние —
`0x004ABCFB`, `0xC0000005`. Артефакты:
`local-data/native-validation-fast-integrated/run-20260814-143617-757/`.

Отдельно проверены:

- `Contextual + startLevel=2`: рабочий `bloom_jeans_from_uzhs.smo` естественно
  запрошен по `Characters\Bloom\bloom_jeans.smo` и получил `Passed` за 18,99 с;
  `local-data/native-validation-contextual-integrated/run-20260814-143744-878/`;
- `FastGeneric`: `Troll.smo` под trigger `Menus\mousecursor.smo` получил
  `Passed` за 5,25 с;
  `local-data/native-validation-troll-integrated/run-20260814-143820-910/`.
- Тот же Troll получил `Passed` за 7,81 с на
  `WinxClubWithDebugMenu/WinxClub.exe`; отдельного каталога `Shaders` рядом с
  patched EXE не было, и isolated workspace корректно нашёл штатные shaders через
  locator. Артефакты:
  `local-data/native-validation-debugmenu-integrated/run-20260814-144005-592/`.
  Это живая проверка того, что допуск определяется внутренним кодом, а не хешем
  конкретной сборки.

Во всех сеансах staging и launch workspace удалены; после пакета и одиночных
проверок не осталось процессов `WinxClub.exe` и дочерних каталогов в
`%TEMP%\SmoNV`.
