# ParticleSystem: общий загруженный класс вместо C# reader

Блок 20 цикла 9 сентября 2026 до 19:00 МСК. Это перенос чтения и инспекции,
не завершение симуляции или графического backend частиц.

## Изменение

`SmoParticleSystemDecoder` больше не разбирает поля SMO. Он возвращает
неизменяемый снимок actual `spParticleSystem` из общего `ResourceGraph`.
Новый `spv_graph_particle` копирует параметры, tag/члены области эмиссии,
начальное состояние пула и ID настоящего borrowed RenderNode. Материал,
туман, alpha и priority приходят из существующего общего Renderable API.
В C# остаются DTO, отображение типов областей и привязка к каноническим
объектам документа. Повторная инспекция использует тот же cached snapshot.

Инспектор теперь показывает исходные defaults, нормализованное движком
направление и raw mode byte, включая `3`. Для cylinder/cone исправлено
перепутанное отображение высоты и радиусов: original serializer задаёт
height перед radius/radius1/radius2. Общий восстановленный алгоритм не менялся.
Старый helper Renderable оставлен единственным private helper в ещё не
перенесённом TextRenderable reader; он больше не обслуживает частицы.

## Доказательства и проверки

Повторно использованы десять архивных original-PC captures CP120:
семь custom region inputs, defaults, tiny и negative direction. SHA256 входов,
состояния и writer output сверены с текущим C++ capture. Исходный EXE заново
не запускался. Через ABI и managed DTO восстановлена та же последовательность
всех определённых в capture байтов состояния.

- Native: ParticleSerialization 46, FullLoader 213 — passed.
- C ABI: 48 проверок, включая 5 guards.
- Managed: 10 fixtures, 67 проверок; отдельный member-order test cylinder/cone
  также прошёл. Исправлена старая тестовая заготовка с недействительной
  forward reference без inline object; правила загрузчика не менялись.
- `SFX/pickup_ptc.smo`: 6 объектов, одна система частиц.
- `SFX/tile_bad.smo`: 127 объектов, две системы частиц.
- `Characters/Bloom/bloom_jeans.smo`: 121 объект, контроль без частиц.
  В трёх полных графах 254 объекта; evidence частиц относится только к двум SFX.
- Пять consumer builds без ошибок и предупреждений. Последующие изменения
  тестовой заготовки и текста Corpus проверены отдельной FormatTests сборкой.

Артефакты: `local-data/results/tools-core-cycle-20260909-1900/particle-reader/`.
Seal: `research/tools-core-particle-reader-block-2026-09-09.json`.
Исходное досье: `docs/research/native-pc-particle-parameters.md`.

## Границы

Сохраняется существующий ограниченный initializer: finite non-looping input,
одна region, 1..1024 частиц. Looping runtime, live emission/simulation, полный
particle renderer и редактирование здесь не реализованы. Дефолтное looping
состояние без поддержанного initializer не выдаётся за готовый emitter.

Typed inspector требует успешного общего графа. Если соседний класс или
initializer не поддержан, возвращается конкретная ошибка; raw field inspection
сохраняется отдельно. Старые числа Corpus 1745/26673 — исторический профиль,
а не текущая полнота общего загрузчика. Его анализатор откажет на unsupported
whole graph до записи результата; полный корпус в этом блоке не запускался.
PS2 runtime и универсальная побитовая эквивалентность x87 не заявляются.
UI layout, релиз и remote publication не выполнялись.
