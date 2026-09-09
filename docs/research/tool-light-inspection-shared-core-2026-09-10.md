# Инспекция LightData через общий reader

Цикл до 07:00 МСК 10 сентября. `SmoLightDataDecoder` больше не реализует
девять scalar/flag полей света, их defaults и правила повторения в C#.
`spv_light_fields_read` передаёт собственную секцию света существующему
`ReadLightFields` из `spLightSerializer.cpp`, который заполняет настоящий
`spLightData`. Восстановленная игровая логика в этом блоке не менялась.

ABI возвращает 48 байт: type, три uint32-проекции флагов, RGBA float4,
intensity/range/hotspot/falloff. Это состояние класса после чтения.
RGBA не перепаковывается в якобы исходный ARGB; Inspector явно показывает
effective state, а маска физически присутствующих полей остаётся отдельной
метаинформацией. Нет загрузки сцены, inherited Node-связей или подключения
LightManager в этом самостоятельном API инспекции.

Исправлены прежние ограничения приложения: конструктор оставляет Enabled=true
при отсутствии поля 8; неизвестные поля пропускаются; позднее повторное поле
побеждает. Nonzero native flag хранится как host bool и в ABI становится 1.
Type остаётся произвольным uint32; scalar IEEE bits не нормализуются.
Это применение уже подтверждённого контракта
[CP88](native-pc-light-serialization.md), а не изменение его ради Viewer.

## Проверка по оригинальному PC

Шесть свежих ограниченных запусков
`research/probe_pc_light_serializer.py data MODE`:

| MODE | Представленная ветвь |
| --- | --- |
| default | Пустая собственная секция и настоящие defaults |
| values | Все девять полей, ARGB → RGBA |
| repeat | Повтор type и неизвестное поле между присваиваниями |
| disabled | Явный false поверх constructor true |
| raw-bits | Type FFFFFFFF, NaN payloads, negative zero, infinity |
| unknown | Пропуск неизвестного поля и последующее известное поле |

Original factory 43FFD0 / Data header reader 4400B0 создают настоящий
DXLight через 4AC000; выполняются reader 440640, writer 440110, fresh-object
reread и фактическое освобождение владельцев. Все original assertions прошли,
876 запрошенных engine bytes в каждом случае, все выделения освобождены.
Состояние read непосредственно из памяти original объекта проецируется на
те же 48 байт; все шесть ABI состояний совпали побитно. Writer и fresh reread
выполняет прежний probe; новая ABI-проверка относится к состоянию после read.

Pristine EXE SHA256:
`3F022480BF55045DA4BF692E4BC8862ED38FC024E8A964A558FBDFDF646DFC4F`.
Профиль micro не изменён: новый guest для каждого случая, 100 000 инструкций
и 2 секунды на вызов, 30 секунд на дочерний процесс, arena 64 КиБ.
Шесть original/ABI сравнений и девять guards заняли 11,62 секунды вместе.
Это время данного короткого набора, а не заявленное ускорение всего цикла.

ABI guards проверяют пустой ввод, отсутствие terminator, обрезанное поле,
trailing bytes, неверный размер scalar, null input/output и предел 16 МиБ.
Эта общая host-граница намеренно строже оригинального malformed reader:
например, CP88 показал original success при отсутствии terminator.
Отказ нашего bounded API не выдаётся за поведение игры на таком вводе.

Локальные артефакты (не входят в Git):
`local-data/results/tools-core-cycle-20260910-0700/light-inspection/`:
`pc-*-1.log`, `abi-1.json`. В JSON сохранены все original captures, входные
секции, 48-byte outputs, SHA256 EXE/DLL и зависимостей проверки.
SHA256 проверенной DLL:
`A41ED2AF61956D1A36B5E34F527F2C732640AAB9DF47FDAC7C5E960E38015415`.
Стенд: `.codex-tmp/check_light_inspection.py` (локальный).

## Managed-подключение и границы

`SmoLightInspectionRegression` использует эти шесть original rows для проверки
direct-section и database field-list путей, маски присутствия, defaults,
неканонического флага, malformed входов и Inspector на
`Characters/Bloom/bloom_projectile.smo`. Итоговый managed набор прошёл48 checks,
включая direct-document transport и его подключение в Inspector. Общий
FormatTests на одном Bloom projectile прошёл647 assertions. Пять зависимых
проектов собраны без предупреждений и ошибок; после правок только ожиданий
тестов пересобран лишь FormatTests.

Дополнительный свежий original capture цвета FF9ED6FF подтвердил RGBA words
`3F1E9E9F 3F56D6D8 3F800000 3F800000`. Первоначальное ожидание в старом общем
тесте было вычислено через деление и отличалось на один бит в green. Исправлено
только ожидание по оригиналу; shared reader и ABI уже совпадали с ним.
Доказательства: `pc-color-ff9ed6ff-1.log`, `abi-color-ff9ed6ff-1.json`.

Direct-section и direct-document API сохраняют исходное framing; Inspector
использует actual document bytes. Field-list overload служит
потребителям наблюдений из базы, у которых нет пригодных исходных offsets:
оболочку полей строит общий `spDataBlockSerializer`. Reader-valid ID31 и
другие ранее известные lossless-формы, невыразимые original writer, остаются
явным отказом этого overload; direct-section путь не требует их переписывать.
Это уже зафиксированное ограничение общего writer, не новая скрытая замена.

Полная загрузка произвольной сцены, новое GPU-освещение, PS2 ABI-совместимость
и полный корпус данным блоком не заявлены. Предыдущие PC Light/LightData
reader и writer доказательства сохраняются в CP88 и
[CP89](native-pc-light-corpus.md); старые проходы не пересчитаны как новые.
