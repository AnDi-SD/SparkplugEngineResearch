# PC2 Particle: естественный возврат looping Init

10 сентября 2026. Оригинальный producer выполнен на настоящем PC2 `bg.smo`:
**539 active / 0 free**,2695 PRNG draws. Production loop guard не изменён.
Это завершённое evidence начального состояния, не whole-file acceptance,
native teardown или сравнение с ещё не написанным source producer.

## Вход и граница

`local-data/pc-pristine/Media/Menus/bg.smo`,279915 bytes,11 объектов,
SHA256 `5F0B9FFA6EFF16C278E4701EBE0A2518C7496189A036BFCB14519E2B5CD59BA5`.
PC2 platform2, SQLite file5444: `stars-001`, index2/ID3, offset381,size16817,
own section17054..17198. Loop field7 отсутствует; native default loop1.
Authored lifetime float32 `1.7999999523162842`,rate300,plane region,
nonzero direction. **Original count539**, не округлённые540.

Предварительный shared baseline на immutable DLL `928F…D8B93` дошёл через
Texture ID5 именно до существующего Particle Init refusal. Это новый вход,
не продолжение [остановленного platform1 bg_particles](native-pc-particle-loop-init-frontier-2026-09-10.md).

Свежий guest вызывает настоящий whole loader `422B50` на неизменённом файле.
Явная граница `4B980D` находится после естественного `48D206 RET` из
`48D1C0`, перед caps query и оставшейся сценой с соседней256×256 texture.
Native `49BCE0→48C340→4B97F0→48D1C0→48C400` выполнены.
Все refs и world state получены original factories/readers. Startup RTTI,
manager/stream/heap и COM остаются явно объявленной средой existing harness.

Seed helper не вызывался стендом. Перед loader снят original mapped-image
PRNG marker625; первый actual Next вызвал native `413270` один раз.
Это native lazy-default при данном входе, не реконструкция game startup seed.

## Результат и лимиты

Первый fresh child на file profile остановился **ровно на1,000,000 instructions**
в mip filter `61C6A8`,cursor16965,5.1786s,arena123216/131072. Init не начался.
Этот guest не продолжался. Root разрешил существующий character profile
4M instructions/16s и256KiB arena по измеренной потребности texture/pool;
общий child cap30s и32KiB allocation cap сохранены.

Второй fresh child достиг заданной границы: **2,010,836 instructions,6.8558s**,
arena158576/262144,serializer cursor **17198**,ровно конец Particle.
Actual CPU vertex object92B,storage17248B =539×32,links6468B =539×12.
Early snapshot снят до Init; final snapshot — после его естественного возврата
и до teardown. Остановленная внешняя loader frame не продолжалась.

| Состояние | До Init | После возврата |
|---|---:|---:|
| Active / free | 0 /539 |539 /0 |
| PRNG index |625 |199 |
| Delta / clock / accumulator bits |0 /0 /0 |0 /0 /0 |
| Linked slots |539 |539 |

Dispatcher выполнен пять раз: batches128,128,128,128,27. Native Next вызван
2695 раз =539×5; final index199 согласован с четырьмя624-word boundaries.
Сохранены все624 слова PRNG до/после,17248 bytes final records и оба linked rings.
Оба rings reciprocal, обходят каждый slot один раз; final records finite.
Birth time в physical storage убывает от−0.00666661886498332
до−1.7999999523162842; lifetime всех записей1.7999999523162842.
Before records содержат allocator poison кроме инициализированного lifetime0;
эти неизвестные bytes не становятся source defaults.

Actual RenderNode ID2, vtable `6DCAA4`: world position `(0,0,16.50927734375)`,
unit scale, orientation примерно−90° вокруг X, **не identity**. Полные float
bits сохранены; например diagonal Y/Z `5.960464477539063e-08`,
off-diagonal `±0.9999999403953552`. Producer выполняет `420350` один раз,
axis-angle `4620C0`539 раз и world matrix `461D70` пять раз.

Сохранённый capture прошёл отдельную consistency проверку и независимый
read-only review. Эти проверки не воспроизводят RNG/producer и не являются
original/source differential comparison. Raw capture:
`particle-loop-init/pc2-bg-init-run2.json`; compact proof:
`pc2-bg-init-proof.json`, под `local-data/results/tools-core-cycle-20260910-0730/`.

## Следующий перенос

Faithful minimum включает `spParticleSystem.h/.cpp`: owning records и ring,
pool reset, Init wrapper и producer; затем serializer hookup, shared axis-angle
helper `4620C0`, C++ capture/tests и original comparer. Общий PRNG уже
[унифицирован](tool-shared-function-particle-random-2026-09-10.md);
второй singleton или per-object reseed не нужны. Existing region samplers,
`420350`/`461D70` math должны переиспользоваться.

До включения общего loop path нужны directed original comparisons:

- Zero direction с sphere-generated velocities; nonzero direction и near-parallel
  basis; special90° angle. Сейчас выполнена только одна nonzero ветка.
- WorldSpace=false, scale/translation/rotation и точные float spills скорости,
  axis-angle и финального world transform. В original nonzero ветке направление
  сначала проходит `420350` с world orientation; при WorldSpace=true готовая
  velocity затем снова преобразуется world orientation. Этот порядок нельзя
  заменять предполагаемой «однократной правильной» трансформацией.
- Counts127/128/129 и256. Static CFG последнего batch использует `count&127`;
  поведение при кратности128 требует отдельного исполнения. Нельзя заранее
  заменять его «правильным» полным batch.
- Все32 defined bytes записей, порядок ring, full PRNG state/index и output
  cursor; затем existing non-loop guards/ownership и actual source whole loader.

Один positive539 case не разрешает объявить эти ветки доказанными. Faithful port
с указанными проверками не включён в оставшееся окно source freeze; unfinished
hookup и заглушка под один SMO не добавлены. Simulation/update/draw остаются
за пределами непосредственного Init consumer.

Manifest с exact commands, snapshots и fingerprints:
[`native-pc-particle-loop-init-pc2-2026-09-10.json`](../../research/native-pc-particle-loop-init-pc2-2026-09-10.json).
