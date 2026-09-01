# Реестр runtime-экспериментов SMO

Статус: начат 28 августа 2026 года. Здесь хранятся только выполненные проходы;
план будущих проверок находится в
[`smo-runtime-validation-plan.md`](smo-runtime-validation-plan.md).

Логи и сводки содержат локальные абсолютные пути и поэтому остаются в
игнорируемом `local-data/validation-results`. В карточке всегда записаны их
относительный locator и hashes входных ресурсов.

## RT-SMO-LOADER-000 — неизменённый fast baseline

| Поле | Значение |
|---|---|
| Статус | `confirmed / Passed` |
| Платформа | PC |
| Executable | `pc-pristine/WinxClub.exe`, SHA-256 `3F022480BF55045DA4BF692E4BC8862ED38FC024E8A964A558FBDFDF646DFC4F` |
| Ресурс | `Media/Menus/mousecursor.smo`, SHA-256 `931B6CF9DA1107F07B086F17F510BB0DD7C1508DC363EDADF289AFDFF7D5E376` |
| Route | `FastGeneric`, trigger `Menus\mousecursor.smo` |
| Media source | `pc-pristine/Media`, принудительный in-process remap |
| Изменение | отсутствует |
| Evidence | `local-data/validation-results/runtime-baseline-20260828-final/run-20260828-213504-503` |

Строгий parser: 18 183 байта, 9 объектов, 1/1 mesh decoded, ноль signature
mismatch. Runtime прошёл `CP02`, `CP03`, `FFPS01`, `FFPS02`, `FFPS03`, вернул
ненулевой native resource и выдержал двухсекундное окно стабильности. Итоговая
длительность Core — 4 043 мс, crash/exception отсутствуют.

Вывод: loader baseline работоспособен. Этот результат является контролем для
`RT-SMO-HEADER-*`, но не доказывает gameplay-семантику произвольной модели.

## RT-SMO-NAME-000 — неизменённый Bloom contextual baseline

| Поле | Значение |
|---|---|
| Статус | `confirmed / Passed` |
| Платформа | PC |
| Executable | тот же pristine build |
| Основной ресурс | `Media/Characters/Bloom/bloom_jeans.smo`, SHA-256 `17184190AAB5C45CFE7EB594B6B166A343CD688D99C236FEA3508CA5809B28CA` |
| Texture sidecar | `bloom_jeans.stx`, SHA-256 `6FA04F4ECCE737DFAC15732D64ED888424890AEE9810AD2A351F9FF356A3D82F` |
| ANM controls | `Bloom.anm` — `C99CD16B05529E2B67C673817C0E4567915C91C3A9500667394CA65D75F948CC`; `Gardenia2Bloom.anm` — `8EFA5E5F95B26133E2A61B3E10EDBD361DDC497F923C3AC0B3A307D48A3DCA5B` |
| Route | `Contextual`, `startLevel=2`, Bloom checkpoints включены |
| Media source | `pc-pristine/Media`, 427 path remaps, 0 ошибок |
| Изменение | отсутствует |
| Evidence | `local-data/validation-results/runtime-baseline-20260828-bloom-contextual/run-20260828-213528-558` |

Строгий parser: 613 109 байт, 121 объект, 6/6 meshes decoded, ноль signature
mismatch; среди объектов 95 `spNode`, 6 `spSkin`, 3 `spCollisionInfo` и 3
`spOBBBV`.

Игра штатно запросила `bloom_jeans.smo`; валидатор подменил только его и
совпадающий `bloom_jeans.stx`. Остальной контекст читался из pristine Media.
Target прошёл FFPS checks, 97 входов в target-scoped `CP08`, ненулевой возврат
`ResourceLoad` и трёхсекундное окно стабильности. Длительность Core — 22 468 мс,
crash/exception отсутствуют.

За тот же сеанс `BuildAssetPath` запросил в каталоге `Characters/Bloom` два
различных ANM (`Bloom.anm`, `Gardenia2Bloom.anm`) и 111 различных SAN. Это
подтвердило реальную runtime-цепочку выбора файлов и стало неизменённым контролем
для следующего эксперимента.

## RT-SMO-NAME-CASE-001 — регистр имени node

| Поле | Значение |
|---|---|
| Статус | `confirmed / case-sensitive` |
| Платформа | PC |
| Executable | тот же pristine build, SHA-256 `3F022480BF55045DA4BF692E4BC8862ED38FC024E8A964A558FBDFDF646DFC4F` |
| Исходник | `bloom_jeans.smo`, SHA-256 `17184190AAB5C45CFE7EB594B6B166A343CD688D99C236FEA3508CA5809B28CA` |
| Mutation | object `[14]`, имя по `0x1B0`: `R_Ankle -> r_Ankle`, ровно один изменённый байт |
| SHA-256 mutation | `039B6504C45C3215FD7434152D6E4AADD8DCDA4746A66139CF52A6AD13B9B053` |
| Изоляция | ANM, SAN, STX и остальной Media оставлены pristine |
| Pristine evidence | `local-data/validation-results/runtime-name-case-20260828-exact-final-baseline/run-20260828-234441-401` |
| Mutation evidence | `local-data/validation-results/runtime-name-case-20260828-exact-mutated/run-20260828-234116-260` |

Обе модели прошли target FFPS/resource-load checkpoints и окно стабильности:
case-only rename не делает SMO невалидным и сам по себе не вызывает crash. Для
поиска обработавший имя код `0x013BBCC4..0x013BBD13` вызывает
`char_traits<char>::compare` и выполняет точный `std::map`-подобный обход.

После target load pristine trace содержит 32 сравнения `R_Ankle`, включая два
точных `R_Ankle == R_Ankle`. Mutation trace содержит 37 релевантных сравнений:
21 для независимо оставшегося `R_Ankle` и 16 для нового `r_Ankle`; для
`r_Ankle` нет ни одного равенства и нет ни одного cross-case сравнения с
`R_Ankle`. Новый ключ проходит отдельную ветвь дерева через `UP-ElbowR`,
`foot_right`, `pickup_sfx`, `shield_master`, `select`, `ps2_key` и `root`.

Дополнительно probes покрыли все 41 статические ссылки PE на `_stricmp`, две на
`_strcmpi`, одну на `lstrcmpiA`, а также `std::string::compare`, `strncmp` и
exact comparators; ни один case-insensitive вызов эту пару не связывает.

Вывод: PC lookup node/animation names регистрозависим. `r_Ankle` и `R_Ankle`
являются разными ключами; отсутствие crash означает только успешную загрузку
контейнера, а не сохранение track-to-node relation.

## RT-SMO-NAME-MISSING-001/002 — отсутствующий parent и leaf target

| Кейс | Mutation | SHA-256 | Результат |
|---|---|---|---|
| parent | `[14] R_Ankle -> Z_Ankle`, `0x1B0`, один байт | `B101BDBBCEE4B1EA7D7C6577BAD28B068B110B97F90B28C976D7BADCDA4B8CB9` | `Passed`, 23 449 мс |
| leaf | `[13] R_Toe -> Z_Toe`, `0x198`, один байт | `2ABA41A6ADCA0EF9661A0E86C0EE904547C47A6ED7BC35DA14360540E4D20E78` | `Passed`, 22 071 мс |

Object `[14]` имеет descendant `[15] foot_right`, object `[13]` не имеет
children. В обоих случаях ANM/SAN/STX оставлены pristine. Обе модели прошли
target FFPS, `CP08`, вернули ненулевой resource и выдержали трёхсекундное окно
без engine rejection или crash:
`local-data/validation-results/runtime-name-missing-20260829-load/run-20260829-000650-409`.

Полный статический аудит 168 Bloom SAN для каждой mutation переводит ровно 168
tracks из exact в missing: pristine `11203/942`, mutation `11035/1110`, case-only
остаётся нулём. Exact parent trace после target return содержит пять сравнений
`R_Ankle` и 55 `Z_Ankle`, но ни одного равенства и ни одного R/Z cross-match:
`local-data/validation-results/runtime-name-missing-20260829-exact-parent/run-20260829-000749-239`.

Transform-binding probe на записи `spTransformTrackEval + 0x10` уточнил поведение:
pristine `R_Toe` и `L_Toe` связываются соответственно со слотами `0x3F` и
`0x3B`, тогда как оба новых имени `Z_Ankle` и `Z_Toe` получают новый отдельный
слот `0xD8`; предыдущее значение поля evaluator равно `0xFFFFFFFF`.
Контрольный descendant `foot_right` после переименования parent по-прежнему
находится точным сравнением и получает прежний слот `0x46`.

Вывод: отсутствующий target является локально допустимой ошибкой binding и не
отвергает весь SMO/animation context. Переименованный узел отделяется от прежнего
name/transform slot, но registry его subtree остаётся целым. Окончательный
bind-pose и наследование world transform ещё требуют активного animation tick:
route `startLevel=2` загружает и связывает модель, но до завершения окна не
вызывает наблюдаемый evaluator method `0x005FEBB0`.

## RT-SMO-NAME-DUPLICATE-001/002 — одинаковые имена в обоих порядках

| Кейс | Mutation | SHA-256 | Результат |
|---|---|---|---|
| поздний duplicate | `[13] R_Toe -> L_Toe`, `0x198`, один байт | `94763A92396D84478AC9F6B703AB18FEB8EED93E646A7F332DDFCBDEAE1FB573` | `Passed`, 21 407 мс |
| ранний duplicate | `[8] L_Toe -> R_Toe`, `0x117`, один байт | `0E5658DDF9124F952A484950682C57F556A619FF2E307B893722AC69BC5EDD23` | `Passed`, 21 502 мс |

Оба порядка проходят native load и окно стабильности:
`local-data/validation-results/runtime-name-duplicate-20260829-load/run-20260829-001037-136`.
Статически каждый вариант теряет 168 exact tracks противоположного имени и
получает два SMO node под оставшимся exact key.

В exact traces вариант с двумя `L_Toe` имеет после target два точных
`L_Toe == L_Toe` и ни одного `R_Toe` equality; зеркальный вариант — два
`R_Toe == R_Toe` и ни одного `L_Toe` equality. В каждой паре возвращается один и
тот же tree node/track slot (`L_Toe=0x3B`, `R_Toe=0x3F`), то есть namespace
сворачивается до одного строкового ключа, а не хранит две независимо адресуемые
записи. Value field `+0x2C` оказался binding-generation marker `0x80 -> 0x81`,
но новый probe перехватил уже саму запись binding: в варианте `R_Toe -> L_Toe`
два разных `spTransformTrackEval` получают один слот `0x3B`; в зеркальном
варианте два других evaluator получают один слот `0x3F`. До записи поле `+0x10`
у каждого равно `0xFFFFFFFF`.

Вывод: duplicate name допустим для loader, убирает противоположный track и на
слое binding имеет политику **all-target**: оба одноимённых объекта получают
одну name/transform-slot привязку. First-only и last-only этим наблюдением
исключены. Итоговые PRS/world-матрицы двух объектов ещё не измерены, поскольку
`startLevel=2` не дошёл до активного animation tick.

## RT-SMO-NAME-BINDING-003 — transform-slot и evaluator probe

| Поле | Значение |
|---|---|
| Статус | `confirmed / binding`; `inconclusive / final PRS` |
| Платформа | PC |
| Executable | `pc-pristine/WinxClub.exe`, SHA-256 `3F022480BF55045DA4BF692E4BC8862ED38FC024E8A964A558FBDFDF646DFC4F` |
| Binding checkpoint | `0x00454628`, запись `mov [edi+0x10], eax` в `spTransformTrackEval` |
| Evaluate checkpoint | `0x005FEBB0`, virtual transform-track evaluator method |
| Binding evidence | `local-data/validation-results/runtime-transform-binding-20260829-full/run-20260829-121344-792` |
| Duplicate calibration | `local-data/validation-results/runtime-transform-binding-20260829-calibration/run-20260829-121058-976` |
| Descendant control | `local-data/validation-results/runtime-transform-binding-20260829-descendant/run-20260829-122234-682` |
| Evaluate evidence | `local-data/validation-results/runtime-transform-evaluate-20260829/run-20260829-122618-722` |

Статическая цепочка вызовов проходит recursive scene traversal
`0x005A34D0 -> 0x005A33F0 -> 0x004545F0`; class ID `0x5DAF152D` и registration
string связывают последний этап с `spTransformTrackEval`. Comparator trace
сопоставляется с записью `+0x10`, поэтому для каждого evaluator одновременно
известны exact lookup, строка и полученный name/transform slot.

| Кейс | Наблюдение binding |
|---|---|
| pristine toes | `L_Toe -> 0x3B`, `R_Toe -> 0x3F` |
| pristine descendant | `foot_right -> 0x46`, exact lookup |
| missing parent | `Z_Ankle -> 0xD8`; `foot_right` остаётся `0x46` |
| missing leaf | `Z_Toe -> 0xD8` |
| duplicate `R_Toe -> L_Toe` | два разных evaluator, оба `L_Toe -> 0x3B` |
| duplicate `L_Toe -> R_Toe` | два разных evaluator, оба `R_Toe -> 0x3F` |

Все пять основных binding-кейсов и отдельный descendant control завершились
`Passed`. Evaluate-probe был повторён для pristine, missing и duplicate: во всех
трёх случаях binding наблюдался шесть раз, но tracked entry в `0x005FEBB0` —
ноль раз. Это воспроизводимый отрицательный результат текущего маршрута, а не
доказательство отсутствия анимации: для final PRS/world matrix нужен debug-menu
или game-flow путь, который действительно запускает animation tick.

## RT-SMO-HEADER-004-001 — serializer version

| Поле | Значение |
|---|---|
| Статус | `confirmed / EngineRejected` |
| Исходник | `mousecursor.smo`, SHA-256 `931B6CF9DA1107F07B086F17F510BB0DD7C1508DC363EDADF289AFDFF7D5E376` |
| Вариант | `0x04: 0x26 -> 0x27`, SHA-256 `FCB05428CFF015FADD190CCBDA7E77C4A4413A2A8FD82C61C35306E8A6428E72` |
| Route | `FastGeneric`, pristine executable/Media |
| Evidence | `local-data/validation-results/runtime-header-20260828-word04-27/run-20260828-214138-518` |

Движок принял `FFPS`, затем до object construction вывел `ERROR: Wrong file
version` из `spSerializerManager.cpp:636`, `Failed to validate file header` и
вернул null. PC disassembly по `0x004222D6` напрямую сравнивает `[header+4]` с
`0x26`.

Вывод: `0x04` — serializer version, допустимое наблюдаемое значение `0x26`.

## RT-SMO-HEADER-008-001/002 — export/session tag candidate

| Кейс | Изменение | SHA-256 варианта | Результат | Evidence |
|---|---|---|---|---|
| `mousecursor-zero` | `0x4823 -> 0` | `6AA9717EEF1B073F734D70A5D9D55026B8AFD05C01ACC342782C3D59987958F6` | `Passed` | `local-data/validation-results/runtime-header-20260828-word08-zero/run-20260828-214154-888` |
| `mousecursor-bit0` | `0x4823 -> 0x4822` | `97068F828738660D3D1C485887FD1AF20E0B6C40B6AEBBE680DFC229F9DF89CE` | `Passed` | `local-data/validation-results/runtime-header-20260828-word08-4822/run-20260828-214203-470` |
| `Bloom-zero` | `0x2EA6 -> 0` | `8A73201D5A8F94CCC8B21A1648D9ED48074D5424157215F777730A47749244CF` | `Passed` | `local-data/validation-results/runtime-header-20260828-bloom-word08-zero/run-20260828-221523-084` |

Первые два кейса прошли FFPS checks, вернули ненулевой ресурс и выдержали две
секунды. Bloom-кейс использовал `Contextual`, `startLevel=2`, pristine Media и
matching pristine STX: 121 объект, 6/6 meshes, вход в `CP08`, ненулевой возврат
и трёхсекундное окно; ANM/SAN оставались неизменными.

Функция проверки заголовка поле `[header+8]` не читает. В schema v3 все 733
канонических PC/PS2 SMO имеют значение `0x0029..0x7FBE`. Корреляция с размером,
DataStart/DataSize, object count и platform mask близка к нулю; CRC32/Adler/FNV
имени и пути не совпали ни разу. Все 29 изменённых PC-working SMO сохранили
pristine-значение. Частые значения группируют разные файлы (`0x0029` — 82,
`0x4823` — 60), что соответствует export/session tag candidate; точный producer/PRNG
пока не доказан.

## RT-SMO-HEADER-010-001 — platform mask

| Изменение | SHA-256 варианта | Результат | Evidence |
|---|---|---|---|
| `2 -> 1` | `92EDA56FC6713CE329334775F8C94175244D0CF7C73BF7970788C50A59C105CD` | `Passed` | `local-data/validation-results/runtime-header-20260828-target-1/run-20260828-214221-114` |
| `2 -> 3` | `16529AB4F6221C6728910576390C8C053D783D540B34DA14D6B1E39D495754CE` | `Passed` | `local-data/validation-results/runtime-header-20260828-target-3/run-20260828-214229-869` |
| `2 -> 8` | `1797A3EE55CFB2831BFCE5B7B48492C40BB29BA7500D4BB162DD00950644BCE2` | `EngineRejected` | `local-data/validation-results/runtime-header-20260828-target-8/run-20260828-214238-983` |

При `8` PC loader принял magic/version, затем вывел `ERROR: File cannot be
loaded on this platform` (`spSerializerManager.cpp:661`) и вернул null. PC-код
по `0x00422372` проверяет `(mask & 3) != 0`; PS2-код по `0x00181EF0` и
`0x00181F00` принимает соответственно биты `8` или `1`.

Вывод: `1=common`, `2=PC`, `8=PS2`; `3` и `9` являются объединёнными масками.
Это поле не является версией формата.

После серии мутаций неизменённый `mousecursor.smo` повторно прошёл baseline:
`local-data/validation-results/runtime-header-20260828-final-baseline/run-20260828-214457-631`.
