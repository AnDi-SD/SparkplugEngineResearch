# PC owner-scoped Remix audit: контракт хуков

13 сентября 2026. Собственный технический контракт адаптера; восстановленная
логика игры не менялась. Задача — связать фактически исполненные
Static/Partition → exact Model → существующий native mesh submit с текущим
scene/root, пока расширение visibility продолжает работать.

**Результат:** три vtable hooks достаточны для наблюдения draw/model границ.
Владельца queued Model необходимо определять по его фактическому `support`
аргументу, а mesh — по конкретному вызову `479DF0`. Одного вложенного TLS scope
Static → Model недостаточно. Два direct destructor hooks дают раннюю
инвалидацию Scene и общей PartitionNode части; они не закрывают все изменения
графа и не разрешают хранить непроверенные borrowed pointers между кадрами.

## Источник и проверка

Новый [машинный evidence](../../research/winx-remix-owner-hook-contract-2026-09-13.json)
содержит версии Python/pefile/Capstone, полный SHA-256 файлов, десять
ограниченных тел с байтами/инструкциями/returns, десять полных bounded vtables
и шесть дополнительных anchors. Чтение PE по ImageBase `00400000`, machine
`014C`; инструкции декодированы как x86. Ни игра, ни native/protected функции,
ни эмулятор, ни GPU в этой проверке не запускались.

| Файл | SHA-256 |
|---|---|
| `local-data/pc-pristine/WinxClub.exe` | `3F022480BF55045DA4BF692E4BC8862ED38FC024E8A964A558FBDFDF646DFC4F` |
| `local-data/Winx Club/WinxClubDebug.exe` | `C27EA9DB4228781A12A90AE808807D4AF1397A7E40DD8F5FFF28F3C87CC62CDB` |

Все записанные диапазоны совпали между файлами. Диапазоны тел заканчиваются
на последнем return/tail jump, без следующего INT3 padding. Это статическая
проверка файлов, не проверка уже изменённой памяти работающего процесса.
Перед установкой нужны существующий image guard и точное сравнение живых
slot/entry bytes; совпавший SHA файла не заменяет проверку slot ownership.

Поведение повторно не исполнялось. Использованы существующие
[CP10 Model](native-pc-model-render-world.md),
[CP11 queues/callbacks](native-pc-renderer-protocol.md),
[CP12 partition/static](native-pc-partition-runtime.md),
[CP13 scene/visibility](native-pc-visibility-runtime.md),
[Scene lifetime](native-pc-scene-world.md),
[CP14 SceneRender](native-pc-scene-render-runtime.md),
[Octree](native-pc-octree-runtime.md), [BSP](native-pc-bsp-runtime.md).
Общий ABI: `Sparkplug/Analysis/PC/SparkplugAbi.h`, Model layout строка 458,
support/static/partition layouts 1258–1325, renderer interface 2458–2479,
Model addresses 3431–3435 на момент чтения.

## Точные x86 границы

Все адреса ниже — VA, числа offsets/addresses шестнадцатеричные.
`camera` и `support` — borrowed pointers, не intrusive references.

| Метод / тело | ECX / stack arguments при входе | Возврат / cleanup | Предпочтительный slot |
|---|---|---|---|
| StaticDraw `44FC00..44FCA1` | `complete+14`; `[esp+4]=camera`, `[esp+8]=forceVisible` | AL boolean; `ret 8` | support table `6E6604`, slot 0, word `6E6604→44FC00` |
| PCPartition Draw `4D72C0..4D7361` | `complete+10`; camera, forceVisible | AL boolean; `ret 8` | support table `6F4528`, slot 0, word `6F4528→4D72C0` |
| ModelRender `479DC0..479E10` | complete Model; camera, **adjusted support** | AL boolean; `ret 8` | primary `6EAA58`, slot 9 / offset `24`, word `6EAA7C→479DC0` |
| Scene dtor `45E5D0..45E71E` | Scene; no stack arguments | void, EAX unspecified; plain `ret` | direct body; deleting slot differs |
| PartitionNode dtor `4264D0..426666` | complete PartitionNode/base prefix; no stack arguments | void, EAX unspecified; plain `ret` | direct common body; deleting slot differs |

`forceVisible` занимает четырёхбайтовое stack место и передаётся неизменным.
Оба exact Static/Partition тела вообще его не читают. Это не основание менять
ABI: caller всё равно передаёт два arguments, callee снимает восемь bytes.
Return проверяется по AL; старшие 24 bits EAX могут остаться от предыдущих
операций. C++ wrapper может использовать `uint32_t(__thiscall*)(...)` и
возвращать полный результат original без нормализации; для счётчика success
проверять только `result & 255`. Не читать full EAX как bool.

Для vtable detour подходит x86 `__fastcall Hook(void* self, void* unusedEDX,
uint32_t arg1, uint32_t arg2)` с original `__thiscall`. Это platform adapter,
не объявление оригинальных C++ header signatures. Hook обязан вызвать original
ровно один раз, сохранить arguments, result и восстановить свой TLS при
выходе/исключении. Unknown identity только пропускает аудит: original вызывается.
Общие support tables не смещать повторно на complete object.

Exact checks: Static primary `6E65E8`, secondary `6E6604`, support=`object+14`;
PCPartition primary `6F4540`, secondary `6F4528`, support=`object+10`.
У обоих `support+70==complete`, `complete+88==scene`.
Model exact primary `6EAA58`; hook только этого table word сознательно не
охватывает derived Model tables, даже если они наследуют `479DC0`.

### Если нужен entry trampoline

Vtable patch предпочтителен для этих трёх exact cohorts. Для direct lifecycle
или отдельно обоснованного entry hook доступны следующие целые prefix:

| Entry | Копируемые bytes | Size / resume | Инструкции |
|---|---|---|---|
| `44FC00` | `A1 68 DB 75 00` | 5 / `44FC05` | `mov eax,[75DB68]` |
| `4D72C0` | `A1 68 DB 75 00` | 5 / `4D72C5` | `mov eax,[75DB68]` |
| `479DC0` | `53 8B 5C 24 08` | 5 / `479DC5` | `push ebx; mov ebx,[esp+8]` |
| `45E5D0` | `6A FF 68 84 1F 6C 00` | 7 / `45E5D7` | `push -1; push 6C1F84` |
| `4264D0` | `6A FF 68 3A FD 6B 00` | 7 / `4264D7` | `push -1; push 6BFD3A` |

В prefix нет relative control transfers. У dtors нельзя копировать только
пять bytes: вторая инструкция `push imm32` будет разрезана. Семь bytes с
последующим jump возобновляют оригинальную SEH установку **до** `mov eax,fs:[0]`.
Абсолютные addresses сохраняются при том же qualified image base. Это пригодные
для рассмотрения displaced bytes; установка/SEH unwind/reentry live здесь не
проверены. Не ставить одновременно entry и vtable hook одной операции без
явного suppression, иначе события задвоятся.

## Порядок callbacks, очереди и native mesh

Static immediate path вызывает support slot 1 (`+4`, prepare) и игнорирует
его AL. Затем по каждому элементу borrowed view собственного intrusive vector
`support+8..+C` вызывает renderable slot 9 (`+24`) с camera и adjusted support.
Model false останавливает цикл. В queue path `renderer[75DB68]+C050!=0`
вместо ModelRender вызывается `456310(renderable,support,camera)`:
Static call `44FC4A`, Partition call `4D730A`. Enqueue false также останавливает
цикл. Запись в queue сама по себе не означает mesh/API submission.

General record20 сохраняет `{renderable,support,camera,materialKey,mesh}`.
`456910` после сортировки вызывает prepare при смене support, затем
`renderable.v24(camera,support)`. Alpha record24 также хранит camera/support/
renderable; `454850` вызывает renderable.v24 после собственного prepare.
Оба flush игнорируют render return. Их исполнение остаётся внутри original
SceneRender, но Static/Partition Draw scope уже завершился. Nine-bucket путь
в этой PC сборке имеет no-op phases; считать его enqueue доказанным submit нельзя.

Exact Model `479DC0`:

1. `479DD1`: original pre slot `20` один раз. False означает successful skip:
   AL=1 без собственного mesh/post. Pre может вызвать callbacks или alpha enqueue.
2. После успешного pre `479DE6` читает **текущее** `model+58`.
   `479DF0` вызывает renderer secondary `+18`, slot 9 / `+24`, с mesh.
   Его адрес возврата — **`479DF3`**. Global renderer — `[75DB68]`.
3. False AL от mesh прекращает Model без post. При success `479E03` вызывает
   post slot `28` один раз; `479E0A` нормализует только AL через setne.

Существующий `research/rtx-remix/winx_native_mesh_source.h:101` уже перехватывает
renderer interface `6F28A0 + 9*4 = 6F28C4 → 4BC670`, создаёт scope с
`mesh, rendererInterface-18, sequence`, а затем вызывает original один раз.
**Не устанавливать второй hook этого word.** Добавить owner qualification
внутри существующей точки; native mesh validation/Resolve сохраняются.

Model scope покрывает pre/post callbacks. Поэтому активный Model scope плюс
совпадение mesh pointer ещё не доказывают, что submit принадлежит основной
геометрии Model: callbacks могут сами отправлять mesh. Для первоначального
exact cohort нужен фактический return address `479DF3`, снятый при входе
существующего `Submit` (например `_ReturnAddress()` непосредственно в wrapper,
не в вызванном вспомогательном getter). Проверить renderer identity и live
`model+58==mesh` **после pre**, в этой точке; ранний snapshot model+58 не
учитывает возможную замену mesh в callback. На mismatch пропустить owner attach,
сохранив обычный native mesh/D3D путь.

## Минимальная связь с scene/root

Текущий `winx_scene_geometry.h:95` scope хранит scene/camera, но не root.
`ReadRegistry` в строке 39 получает **partition root** по
`scene+38 → partitionSystem+1D4`. Это не `scene+14`: последний — SceneRoot
в общем Node tree. Нельзя присваивать одному полю оба смысла.

Начальный owner record может содержать `(scene, system, partitionRoot,
ownerEpoch, frame, support, completeOwner, model, modelCallSequence,
nativeSubmissionSequence)`. Все native addresses borrowed; adapter хранит
числовые identities/копии подтверждённых данных, не принимает ownership и не
вызывает AddRef/Release, Draw, Prepare или callbacks дополнительно.

Предлагаемая минимальная последовательность:

1. На существующем render thread и внутри qualified main
   `scene_geometry::active` сохранить scene/system/partitionRoot и epoch.
   Использовать уже проверяемый registry traversal, не второй игровой traversal.
   На этой стадии сохранять visibility extension как есть.
2. Registry обязан подтвердить принадлежность support **текущему root graph**:
   exact Partition payload из node+78 или exact Static из node vector30,
   node+80==scene, затем exact object/support identities выше. Переходы через
   children58/count5C и Zone60→borrowed local roots входят в текущий обход.
3. Два support Draw hooks дают контекст immediate/queued и наблюдение фактических
   вызовов. Hook Model использует свой support argument, проверяет его membership
   независимо от наличия support Draw TLS; так работают оба flush. Если TLS есть,
   несовпадающий support нельзя молча заменить текущим TLS support.
4. В Model hook выставить nested scope с exact model/support/camera и новым
   call sequence, вызвать original один раз. Actual camera должен совпадать с
   main Scene scope; nested/foreign pass не наследует предыдущего владельца.
5. В существующем native mesh Submit проверить callsite, текущий mesh58,
   renderer/thread, живой scene/root/epoch/frame. При success скопировать owner
   identity в **этот** native submission scope. Последующий Geometry packet
   должен совпадать по mesh/renderer/submission sequence, как текущий source guard.
6. Сначала считать audit events и mismatches, не создавать дополнительные Remix
   instances и не подавлять D3D на основании одного owner совпадения. Существующий
   surface submit остаётся единственным местом API geometry submission.

`ReadRegistry` сейчас использует `set` и возвращает только unique support list.
Это достаточная основа проверки root membership, но **не** unique node ownership:
Static может иметь несколько registration occurrences, а один Model может
повторяться в support vector. Если нужны node provenance/ordinal, сохранить
отдельные relationships при том же bounded обходе, не менять draw selection
и не удалять дубликаты оригинальной логики. После сортировки одинаковых queued
tuples исходный vector ordinal из одного Model call восстановить нельзя.
Для аудита различать реальные вызовы последовательным номером; не выдавать его
за stable serialized instance identity. Не объединять два Model calls только
потому, что их pointers одинаковы.

## Lifecycle и инвалидация

| Class | Deleting slot 0 | Direct destructor |
|---|---|---|
| Scene `6E7358` | `45EB50`, call `45EB53→45E5D0` | `45E5D0` |
| PartitionNode `6DCB08` | `426890`, call `426893→4264D0` | `4264D0` |
| Octree `6E4420` | `449AE0→449420` | `449420` writes own vtable; `449426→4264D0` tail jump |
| BSPNode `6EBA30` | `480450`, call `480453→480180` | protected entry `480180→[13B3248]`; freshly decoded body not claimed |

Каждый перечисленный deleting wrapper принимает stack `flags`, вызывает dtor,
при `flags&1` освобождает complete pointer через `412420`, возвращает исходный
pointer в EAX и делает `ret 4`. Это **не** ABI direct void dtor. Patch только
base PartitionNode deleting slot пропустит Octree/BSP deleting slots.

Для минимальной общей инвалидации предпочтительны direct `45E5D0` и `4264D0`,
до вызова оригинала: после него graph/vector pointers могут быть освобождены.
Scene destructor снимает registration, освобождает Node root и referenced
partition systems; PartitionNode destructor сначала уведомляет reverse
registrations, затем уничтожает payload/children и освобождает vectors/refs.
Наблюдатель только помечает соответствующую adapter generation недействительной.
Нельзя заменять cleanup вызовом root virtual `80=4269C0`: CP12/13 доказывают,
что это non-notifying transfer reset, а не универсальный destructor.

Common `4264D0` вызывается также для children; инвалидировать связанные tracked
relationships либо консервативно весь текущий root epoch. Не считать любой
this на этой границе главным root. При деструкторе derived исходная динамическая
часть могла уже завершить cleanup; проверять tracked pointer/generation, а не
требовать свежую derived vtable. Octree→common edge статически подтверждён.
BSP inherited destruction подтверждён историческим CP19; его protected путь
до конкретного common hook в этом новом read-only срезе не декодировался.
Для ранней BSP invalidation можно отдельно перехватить проверенный deleting
slot, но тогда common event должен быть idempotent, без двойного cleanup.

Деструкторы не покрывают removal, scene partition switch `45F4D0`, mesh/material
replacement, transfer/reset и адресное повторное использование. Поэтому каждый
active scene/root, каждый frame/device epoch и submission остаются отдельной
границей. При изменении `scene+38` или `system+1D4`, при tracked destructor,
Reset/scene exit или несовпадении текущих identities старые packets отвергать.
До исследования update hooks долгоживущие owner records не разрешают обходить
проверку текущего membership. Отсутствующий между кадрами объект не объявлять
уничтоженным только по отсутствию draw.

## Что ещё требуется проверить при внедрении

Это технический контракт, не live PASS. Нужны bounded CPU tests scopes/guards и
один согласованный main-scene audit: immediate, general queue и alpha queue;
pre false/mesh false/post false; callbacks с чужим submit; повтор Model в разных
supports; nested Scene/Model; foreign camera; смена root; destructor до конца
frame; reset и reuse адреса. Счётчики должны отделять observed support draws,
enqueue-only, Model calls, qualified own-mesh calls, native geometry accepted
и фактические API submits. Проверка не должна сама повторять игровые callbacks.

Не закрыты универсальные stable instance identities, все derived/custom
renderables, изменение ownership в произвольном callback, защищённый BSP
destructor edge и persistence геометрии между кадрами. Текущая работа лишь
создала два новых evidence/contract файла; production и установка игры не менялись.
