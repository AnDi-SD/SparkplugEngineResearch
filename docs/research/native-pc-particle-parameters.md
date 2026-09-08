# PC ParticleSystem: параметры, writer и первая целая сцена

CP120, 8 сентября 2026. Проверяемый PC EXE:
`3F022480BF55045DA4BF692E4BC8862ED38FC024E8A964A558FBDFDF646DFC4F`.
Это самостоятельное нативное подтверждение поверх старого
[разбора wire layout](smo-class-sp-particle-system.md), без переноса PC credit на PS2.
Следующий шаг выполнен в [CP121 — семь sampler functions и MT19937](native-pc-particle-sampling.md).

## Выполненные оригинальные функции

`48D8D0` создаёт `spParticleSystem` (264 bytes, vtable `6EC40C`), constructor
`48D6F0` вызывает настоящий lazy manager `45A530` через global `75DB84` и
регистрирует объект. `49BBB0` создаёт serializer (20 bytes). Его secondary
table `6ED47C`: writer `49C8D0`, index `49BC90`, reader `49BCE0`.
RTTI initializer `6D4AA0` задаёт serializer ID `047F310F` и parent
`spRenderableSerializer`; target ID — `5AFA1A4F`.

Reader действительно выполняет inherited `47FBA0`, собственные fields,
`48C340`, создание CPU vertex storage, список свободных частиц `48BE50`,
factory render support `4AC7D0`, initializer `4B97F0` и renderer query `4AD4D0`.
Последний вызывает D3D9 `GetDeviceCaps` (device vtable offset `1C`), читает
mask `100000` по offset `8C` возвращённой структуры. Стенд предоставляет
304 bytes caps и оба значения этого бита; тело проверки остаётся оригинальным.

В первом scout отсутствовала renderer secondary table; в следующем — внешний
GetDeviceCaps. Каждый остановленный guest отброшен, новый запуск начинался с
объявленным входом. Никакая остановленная engine function не заменена success.

## Уточнения поведения

* Constructor acceleration — `(0,0,1)..(0,0,1)`, direction — zero,
  velocity `0..1`, angle `0..180`, scale `1..1`, colors `FFFFFFFF..0`,
  emission/lifetime `-1..1`, flags `1,1,0`, rate100. Sphere radius0, region none.
* Field1 нормализует direction через `41D2D0`: length не больше float0,001
  превращается в zero. Сравнены zero, threshold, positive и negative direction.
  Source использует double intermediates; все возможные x87 rounding cases
  этой нормализации отдельно не закрыты.
* Writer сравнивает acceleration/direction с default по каждой координате
  с допуском float0,001 (`439290`), остальные ranges — по собственным точным
  scalar/bit comparisons. Scale `1..1` пропускается. Radius пишется всегда.
* Loop/world-space пишутся только когда равны zero; iterative — если ненулевой,
  с сохранением исходного byte. Inherited alpha-sort/priority пишутся всегда.
* Fields0 и5 используют UInt8 Begin/End, хотя field5 имеет всего8 bytes;
  ranges2/3/4/6 используют Fixed8. Семь regions также используют UInt8 Begin/End.
  Tags point1, box2, sphere3, plane4, disk5, cylinder6, cone7 отличаются от field order.
* Field19 сохраняется только для ненулевого render node. Native reader просто
  сохраняет borrowed pointer в `+5C`; destructor его не освобождает. Source weak
  pointer предотвращает цикл `RenderNode -> ParticleSystem -> RenderNode`.
* `48C340` берёт emission duration, если она неотрицательна, иначе lifetime,
  умножает на rate, truncates и ограничивает unsigned count до65536. Это не
  `min(emission,lifetime)`. При loop0 active count0/free countN, counters zero.
  Native zero-count list initialization небезопасна; source явно требует1..1024.

Source пока завершает ReadPayload только для конечных non-looping parameters
с одной region. Looping initialization вызывает `48D1C0` и уже испускает частицы;
simulation/update/draw, reinitialization/clone и произвольные malformed inputs
остаются открыты. Writer поддерживает также constructor defaults и flags;
наличие writer не означает готовность полного SMO SaveResources.

## Проверка

[Directed comparer](../../research/compare_pc_particle_parameters.py): **20/20**,
включая default writer, семь regions при обоих caps, omitted defaults,
vector threshold и три дополнительных direction. Четыре workers: **6,09 с**.
Совпали2908 state bytes и2420 writer bytes; освобождены все460 allocations.
Максимум23699 инструкций на вызов и68112 arena bytes.
Каждый случай сравнивает весь определённый parameter/initial-pool state и
весь writer output, затем выполняет оригинальные destructors и manager cleanup.
Raw captures остаются в ignored local-data.

[Whole loader](../../research/probe_pc_scene_file_profile.py) выполнил неизменённый
`SFX/pickup_ptc.smo`,4733 bytes,
SHA-256 `D8CB2C06BCAC278B54E3335129E2FFA851244159DCA8ADDC07F83D251A295757`.
Совпали **6 объектов,543 state bytes,85 material-layer bytes,5460 texture bytes**
(всего6088), включая обратную ссылку на render node и все6 mip levels32×32 texture.
**577950 инструкций**,1,87 с на whole load; **138 allocations освобождены**,
arena94400/131072 bytes. COM texture/device references сбалансированы.
Mesh combiner отсутствует: вызванный PC batch hook не создаёт его без mesh.

Source guards проверяют malformed extents, повторную region, нечисловой direction,
неподдержанную looping initialization, zero count и отсутствие ownership cycle.
Профиль `pc-particle-parameters` воспроизводит directed comparisons;
`pc-scene-file-profile` включает whole pickup scene.

Full build81,55 с; CTest **63/63**,58,32 с; Python **102/102**,3,766 с.
Particle guard suite32 assertions. Scope audit83 class/platform pairs,
без пропусков; первый docs audit423 Markdown/170 JSON/1535 ссылок, без ошибок.

Исходники:
[spParticleSystem](../../Sparkplug/Code/Sparkplug/spParticleSystem.h),
[serializer](../../Sparkplug/Code/Sparkplug/spParticleSystemSerializer.cpp),
[COM input и native capture](../../research/pc_particle_fixtures.py).
