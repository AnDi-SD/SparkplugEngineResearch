# Общий LensFlare reader: PC поведение и PS2 владение

Блок 18 цикла до 19:00 МСК 9 сентября. Восстановлены используемые читателем
части spLensFlare, spQuad и spLensFlareSerializer. C# parser удалён; инспектор
проецирует actual objects того же ResourceGraph. GPU flare/occlusion query pass,
clone и writer ещё не реализованы.

## Оригинал и исправление прежнего предположения

PC factory `4D7BD0` создаёт LensFlare `EC` bytes, vtable `6F4594`, ID435370B5,
база Renderable4FDA4542. Serializer72350266→RenderableSerializer4D694D82,
factory4D7E90, secondary reader4D9010. spQuad073411BC — actual spBaseObject,
factory4CD7F0, размер48, vtable6F3364. Это частичные восстановленные классы,
а не утверждение о восстановлении всего renderer.

Reader после общего Renderable читает следующие поля:

| Field | Реальное поведение |
|---|---|
| 0 | material reference + color/distance/scale; nonnull заменяет один primary element, NULL сохраняет прежние значения |
| 1 | UInt32 count, затем count таких записей; размер массива меняется, каждая запись задаёт свой материал и raw tail, включая NULL |
| 2 | два raw float32: radius и speed; нет проверки конечности или знака |
| 3 | заимствованный RenderNode pointer; NULL допустим |

Старый C# reader считал field0 списком, а field1 одним optional glare. Все шесть
корпусных объектов имели field1=0, что скрывало ошибку. В настоящем reader это
нулевой **count**, не null material ID. Original count2, повторное уменьшение
до1/увеличение до3, NULL primary и четыре material ownership refs подтвердили
структуру. Новая модель инспектора содержит Primary и Elements; исторический
semantic key `lens_flare.glare` сохранён для совместимости БД, описание исправлено.

Конструктор задаёт radius/speed=1; primary color=FFFFFFFF, distance=0, scale=1.
Материал primary находится в actual embedded spQuad. Дополнительные записи
имеют размер58 и такой же Quad. Сфера по PC4D74B0 имеет центр0 и radius,
умноженный на точный float32 `3FB504F3` из6F458C. Native draw не подменён
обычным Model и не подключён к GL pass.

## Защищённый деструктор и помощь PS2

PC deleting wrapper4D7BB0→4D7A30 достиг исходного instruction cap в88B422.
Этот вызов не повторялся. Первоначально подключение владения было отложено,
о чём сообщено пользователю. Независимые reader probes завершаются, но их
целевой LensFlare остаётся в памяти guest до завершения процесса; native cleanup
этих объектов не заявляется.

PS2 registration/factory1B46D0 и vtable490570 подтвердили тот же класс с layoutE0.
Незащищённый destructor1B4270 освобождает массив через18CC60, embedded Quad
через15BF30, затем базовый Renderable1A9A00. PC4CD770 отдельно показывает
порядок release Material+14, VertexBuffer+10. Свежий original-PC spQuad test
выполнил factory и destructor: Quad освобождён, material refs2→1, внешний
владелец остаётся жив; последующий cleanup освободил все native allocations.
Так ownership закрыт прямым PS2 кодом и отдельным PC подтверждением, без
заглушки защищённого метода. Полный PC LensFlare destructor остаётся непроверенным.

Дизассемблер сохраняет неизвестные R5900 MMI как raw words и отдельно печатает
SQ/LQ; первоначальный обычный MIPS64 вывод мог ошибочно называть эти opcodes
инструкциями другой ISA. Для выводов использованы исправленные ranges.
PS2 исполняемый файл не запускался; это статическое подтверждение, не guest test.

## Проверки и реальные ограничения

- Пять original-PC reader captures: empty, raw scalars, count2, shared materials,
  repeated resize. Все состояния совпали с общим C++. Максимум меньше64 KiB.
- Отдельный PC Quad ownership/cleanup capture завершён полностью.
- Через ABI проверены три состояния тех же original scalar captures и целый
  тестовый FFPS из RenderNode/LensFlare/MaterialData с двумя дополнительными
  slots и общей идентичностью material. 21 check, из них15 guards.
- C# тот же graph: четыре fixtures,20 checks, включая NaN bits и отображение
  counted array. Native LensFlare15 / FullLoader213; пять consumer builds.
- battle_01 и Gardenia03 прошли прежний LensFlare registration frontier,
  но оба остановились на OcclusionVolume43D24430. Они **не** выданы за успешно
  загруженные сцены. LensFlare inspector этих документов пока также недоступен;
  raw fields остаются видимыми. Следующий приоритет — настоящий Occlusion Init.

Reader имеет host limit4096 элементов и bounded field extents. Wrong-type refs
отклоняются явно; это host guards, не дополнительные правила оригинала.
Специальный flare pass, query composition, runtime Quad upload/draw и authoring
ещё открыты. Макет UI не менялся. Восстановленных материалов или объектов
заглушками не заменяли; новых успешных реальных уровней в этом блоке нет.

Отдельно найден старый TextureData в test_world_navmesh: такая форма без source
wrapper уже проверена CP116. Original reader может вернуть1 с диагностикой
чтения, оставив texture uninitialized; это не доказательство загрузки пикселей.
Автоматическое исправление ресурса или искусственная инициализация не добавлены.

DLL: `5677288503284A8BAAD8540B97F25A9EA1D24F8732E751B63B6CC76A08C666B6`.
Результаты: `local-data/results/tools-core-cycle-20260909-1900/lens-flare/`.
Состав: `research/tools-core-lens-flare-block-2026-09-09.json`. Релиза нет.
