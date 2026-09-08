# CP128 — общий SAN для Viewer и Exporter

Checkpoint цикла **8 сентября 2026, до 19:00 МСК**. Фиксированный измеритель
остаётся **16/16**: расширение проверенных вариантов и исправления существующих
операций не прибавляют новые проценты. Полное восстановление классов не требуется.

## Изменение инструментов

Viewer 0.6 и Exporter 0.7 рабочего дерева используют один подготовленный
PC SAN sampler. Ранее неподдерживаемые payloads превращались в пустые списки.
Теперь representations 1–4 разбираются в установленном контракте: packed linear,
packed cubic/Squad, независимые scalar-оси, смешанные 3/4 для вращения.
Реальный `bbush.san` сохраняет неединичный cubic scale; VMD по-прежнему обязан
его отклонять из-за ограничений своего формата.

Использованы оригинальные PC-результаты [CP127](tool-san-keys-2026-09-08.md):
reader `0x43DB90`, подготовка коэффициентов, sampler `0x479290`. Повторный запуск
EXE для тех же 195 поз не потребовался: восстановлены точные SHA-проверенные
payloads. Сохранены двухключевой endpoint, small-angle SLERP, радианы и порядок
`qZ*qY*qX`. Коэффициенты готовятся один раз, интервал ищется бинарным поиском.

`SmoAnimationChannel`, `SmoAnimationBinding`, `SmoAnimationBaker` — адаптеры
инструментов, не оригинальные классы/ABI. Нормализация quaternion, ограничение
Log и bounded single-key cubic — явные host guards из CP127.

Viewer создаёт таблицу привязок при выборе SAN. Раздельные PRS одноимённых
дорожек объединяются; конфликт одного свойства сохраняет первое с предупреждением.
Это политика инструмента, не доказанное исходное правило конфликта SAN.
Имена регистрозависимы. Exporter применяет дорожку ко всем точным совпадениям
узлов согласно подтверждённому [слою привязки PC](smo-class-sp-node.md).
Итоговый игровой tick duplicate namespace этим тестом не заявляется.
Повреждённый выбранный SAN останавливает старую анимацию Viewer; нулевая
длительность остаётся статической. Ошибка SAN останавливает GLB/FBX до записи.

## Экспорт и точность

GLB/FBX получают 30fps grid, source key times, соседние float32-времена перед
ключами и дополнительные реальные выборки за `1/1500` с до границ, когда для
них есть место. FBX сохраняет эти времена при переводе quaternion в Euler.
Между ключами используется приближённая интерполяция получателя; исходные
cubic/Squad не переносятся как нативные кривые.

Отрицательная шкала сдвигается к нулю **до** выбора целевых времён.
Ранее `BitDecrement(0) + 1 == 1` сливал ключи и терял endpoint jump. Теперь
значения границы и предела слева получают разные целевые времена;
неразличимые после float32-сдвига исходные ключи отклоняются явно.

Blender 4.5.3 объединяет соседние float32-ключи обоих форматов при импорте.
В GLB два времени/значения присутствуют; после импорта остаётся один ключ.
Это согласуется с [deduplication F-Curve в исходниках Blender 4.5](https://raw.githubusercontent.com/blender/blender/blender-v4.5-release/source/blender/blenkernel/intern/fcurve.cc).
Выборка за ~0,67 мс сокращает размазывание скачка в проверенном получателе;
точность внутри этого интервала не гарантируется.

Первоначальный negative boundary test сохранён как failed. Финальный report
отдельно содержит **192 passed required checks** и **12 несовпадений диагностики
соседнего float32-времени** (6 деформируемых meshes × 2 формата). Максимальное
диагностическое расхождение — 60,602 единицы сцены; это не прошедшая проверка.
На grid и дополнительных контрольных временах максимум — 0,0001165.
Статус этого случая: `passed-with-receiver-subframe-limit`.

## Валидация

Пути таблицы относительны `local-data/results/tool-cycle-20260908-1900/`.
Игровые assets, generated models и дампы не включаются в Git.

| Проверка | Результат | Report |
|---|---|---|
| Общий C# sampler | 29 cases, 8 real SAN, 195 PC PRS, 2 669 checks; max `4.7684e-7` | `shared-san-v1/report-v3.json` |
| Настоящие WPF handlers | 121 checks; 3 SMO, 7 SAN, 43 позы, zero-duration и invalid selection | `shared-san-gui-v1/capture.json` |
| Геометрия окна / независимый NumPy FK | 419 mesh/time checks; max `0.0000627` | `shared-san-gui-v1/report.json` |
| Production GLB/FBX / importers Blender | 7 clips, 14 files, 15 238 export keys, 6 070 mesh/time checks, 1 868 446 сравнений вершин; max `0.0003595` | `shared-san-export-v4/blender-report.json` |
| Negative endpoint | 9 новых PC-поз; 192 required passed, 12 declared subframe mismatches | `san-negative-export-v3/blender-report.json` |
| Новый bridge / прежняя alpha | 70 export + 489 071 independent checks; max alpha `0.000007645` | `fbx-alpha-cp128/blender-report.json` |
| Настоящий CLI | malformed, unbound, missing SAN: 3 отказа, оба прежних GLB/FBX неизменны | `san-cli-rejections/report.json` |
| Существующая регрессия Knut | 68 assertions | вывод `SmoExporter.FormatTests` |

В четырёх rare fixtures эталон каждой из 61 поз взят из оригинального PC-кода.
Ordinary Icy/Knut и real bbush используют общий sampler, отдельно сопоставленный
с PC. Bind/mesh/palette взяты из ранее проверенного export scene: это проверка
применения анимации, не новое доказательство native mesh/skin decoder.
Погрешность: прежний критерий `1e-5 + sceneExtent * 8e-6`.

Viewer тестируется через настоящие load/selection/slider events; измерена CPU-копия
геометрии. OpenGL host инициализирован, но GPU pixels и OS file pickers здесь
не проверяются. Окно пользователю не показывается.

Decoder: один PC FFPS 0x26 объект, SAN ≤64 МиБ, ≤100 000 keys/ось. Пустые
cubic/scalar, неизвестные поля/representations, неконечные используемые значения
и плохие времена отклоняются. Export set ≤500 000 vector/quaternion keys,
включая duplicate targets; native FBX отдельно ограничивает rotation sampling
500 000. PS2 SAN, events, actor looping и точный subframe playback не заявлены.

## Скорость и PS2

C# regression: 0,36 с/~35 МиБ; Viewer workflow: 4,62 с/~204 МиБ;
подготовка 14 export files: 1,86 с/~59 МиБ; независимый импорт: 17,36 с.
Повторно используются SHA-привязанные PC-позы и общие baked curves для одинаковых
targets. Проверки выбираются по изменённому маршруту; полного corpus scan нет.

[PS2 companion](tool-ps2-san-2026-09-08.md) независимо подтвердил интервальную
ветку: 65 ограниченных запусков scalar leaf за ~0,72 с. Это частичное
свидетельство; широкий class ledger не изменён.
SHA исходников, receiver и reports:
[validation CP128](../../research/tool-shared-san-validation-2026-09-08.json).
