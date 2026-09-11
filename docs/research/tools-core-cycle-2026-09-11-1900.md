# Цикл ядер tools 11 сентября, до 19:00 МСК

Начало: 11 сентября 07:50 МСК. Общий ориентир готовности ядер — 14–15 сентября.
Пользователь возобновил разработку и разрешил самостоятельно применять
ускоряющие технические решения. Полный разбор EXE остаётся долгосрочной целью.

Исходный срез: [семь ядер на 10 сентября](tools-core-migration-status-2026-09-10.md).
Последующий цикл EXE дополнил доказательства, но не изменил production C++.
Повторно считать уже перенесённые readers/inspectors отсутствующими нельзя.

Первый общий пакет — один host encoder FFPS/FAT вместо отдельных writers
Editing/Importer/LVLcreator (TextureTool использует общий Editing). Прямой
original whole-file writer не установлен; сохраняем это ограничение. Новое
разрешение пользователя позволяет реализовать ранее описанную техническую
замену без отдельного ожидания. Цель — точное сохранение существующих outputs,
raw names, ID и opaque payload, последовательная запись без полной лишней копии.
Проверка — реальные операции старых потребителей, общий reader и адресная
совместимость с оригинальным чтением; время и allocations измеряются отдельно.

Следующая очередь уточняется по результату первого пакета: общая sort policy
и Occlusion, перенос Particle Init, свет/multipass, остатки операций импорта.
Ядро считается готовым по законченным необходимым операциям, не по числу тестов
или экспертному проценту EXE. UI и исторические спорные случаи учитываются явно.

## Блок 1 — общий envelope encoder

[Результат и границы](tool-shared-envelope-2026-09-11.md): единый host FFPS/FAT
encoder подключён к трём прежним writers, leaf replacement и общему native
graph producer. Подтверждены 23 побайтных сравнения операций, 17 boundary checks,
три actual PC whole-reader, native suites и настоящий TextureTool (2 PNG/6 замен).
Новые баллы изученности EXE не начисляются: это внедрение и согласованная
техническая замена. Устойчивое ускорение runtime не подтверждено, duplicate
encoder убраны; полный дополнительный data buffer для Stream не вводился.

Следующий пакет — принадлежащий runtime сцены LightManager и чтение actual
RenderNode light cache. Подготовка уже описана в
[досье Icy](tool-viewer-light-cache-boundary-2026-09-10.md); новые алгоритмы выбора
света не требуются. UI redesign не нужен.

## Блок 2 — свет загруженной сцены

[LightManager подключён](tool-viewer-scene-lighting-2026-09-11.md) к общему native
runtime, managed API возвращает cache по конкретному RenderNode. 56 адресных checks
на настоящем Icy, native RenderNode/SkinRender 2/2 и сборка Viewer прошли. Игра и
reconstructed selector не изменены; активация и окончательное обновление после позы —
явная host preview policy. GPU использование выбранного света остаётся следующим шагом.

Далее — общая версия CRT sort для geometry/Alpha и подключение Occlusion Init.
Ожидание выбора из прежнего proposal снято новым разрешением пользователя. Историческая
версия игровой CRT неизвестна; выбранная версия и её доказательства будут названы явно.

## Блоки 3–4 — общий sort и Occlusion runtime

[Версионная CRT policy](tool-shared-sort-policy-2026-09-11.md) реализована один раз
для geometry и Alpha. Десять original cases дали точное совпадение перестановок
и 222 comparator calls. [Occlusion Init/reader/world](tool-occlusion-core-2026-09-11.md)
перенесены и подключены к общему PC loader: race_02 теперь загружается целиком
(7513 objects/319 Node/2 Occlusion), создаёт сцену и LightManager за 0,208 s.
Пять original fresh objects и их world updates сравниваются по 1330 словам полного
поддерживаемого состояния. Ошибка harness cleanup исправлена; свежий пакет прошёл
с освобождением всех tracked allocations. Старые runtime lifetime границы сохранены.

## Блок 5 — начальный CPU-пул частиц

[Общий Particle Init](tool-particle-cpu-init-2026-09-11.md) подключён к reader
и доступен через C ABI/C#. Настоящий PC2 `Menus/bg.smo` теперь загружается:
539 первоначальных записей совпадают с игрой побитно. Десять native cases
покрывают 17818 original words; шесть новых cases собраны одним guest за 1,273 s.
Старые 10 managed fixtures/67 checks не нарушены. Исправлены найденные сравнением
ошибки новой ветки вставки в кольцо и подготовки world input в тесте.
Frame simulation/render, capacity выше 1024 и PS2 ещё не закрыты; пределы явные.
