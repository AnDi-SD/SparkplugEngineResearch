# Снаряды и менеджеры: два семейства PC/PS2

10 сентября 2026. [Контракт22 классов](../../research/projectile-contracts-2026-09-10.json)
содержит независимые factory/size/vtable/getter проверки обеих платформ,
исходные байты и file offsets. Pristine PC SHA256
`3F022480BF55045DA4BF692E4BC8862ED38FC024E8A964A558FBDFDF646DFC4F`, PS2
`198313352DBF4FA26FF8C5D509F6783FC32F9B504A627E416323C5FFBBFFE8FE`.

## Объекты снарядов

Десять классов: `wxProjectile`, `wxFetidBreathProjectile`,
`wxGolemLitProjectile`, `wxGolemShockwaveProjectile`, `wxPiercingWindProjectile`,
`wxStarsProjectile`, `wxStingerProjectile`, `wxStompProjectile`, `wxTidalFlame`,
`wxWebSpitProjectile`. Это отдельные варианты снаряда; траектории, урон и
полный игровой эффект каждого пока не закрыты. Их размеры совпадают между
PC/PS2: соответственно236,272,268,328,476,552,248,340,424,256 байт.

База имеет11 slots: PC vtable6F6B04, PS249B2B0. Все девять PS2 собственных
конструкторов первым вызовом используют Projectile2C5030. Ранее доказанное
различие сохранено: engine registration называет wxEntity, фактическая
конструкция базы идёт через spBaseObject; это не новое обнаружение и не повод
исправлять оригинальную регистрацию. См.
[исходное доказательство](native-entity-direct-family-2026-09-10.md).

Старая PC остановка Clone405C22 оказалась зависимостью от **spAnimationManager**.
Copy4FFC10 создаёт member+20 как новый spActor через5A3500. Его spController
constructor423010 требует уже созданный manager75F880. Вместо подстановки
регистрации используется оригинальная manager factory454640. Базовый снаряд
с этим контекстом прошёл за0,477 с, девять потомков — за3,913 с тремя workers.
Все десять factory/RTTI/clone/two deletes прошли:50 class operations. Созданные
Actor сняты с реестра, затем реальный manager удалён и global обнулён.
Полный startup игры не утверждается.

PS2 Copy2C3450 также выделяет54 байта и вызывает spActor1184B0; его базовый
spController11A110 вызывает register118D80 с manager global49F858. Это
независимое подтверждение зависимости; PS2 полный Actor lifecycle здесь
не исполнялся. Подробный прежний PC контракт используется повторно:
[spController](native-class-sp-controller.md),
[spActor](native-class-sp-actor.md),
[spAnimationManager](native-class-sp-animation-manager.md).

### Copy: шесть пар и два PC опыта

[Probe](../../research/probe_projectile_copy.py),1,613 с. PC Copy от входа,
PS2 own prefix после успешного inherited copy; оба останавливаются у настоящего
allocator нового Actor. Переносятся слова10,28,34,38,40,44,48,4C,50,54,2C,9C,
байты30/A0 и retained pointers14/58/5C/D4; offsets одинаковы на PC/PS2.
Остальные байты назначения сохранены. Float-encoded слова проверены как
побитовая загрузка/запись, не как арифметика.

У каждого retained pointer сначала уменьшается uint16 old count, затем
увеличивается new count; подтверждены null, retain, release без последнего
владельца, замена, совпадение указателей при count3 иFFFF→0. Последнее
освобождение не подменяется и этими prefix cases не проверялось. Guard охватывает
целые source/target и все literal reference records.

Два дополнительных **полных PC Copy** создают source Actor оригинальным Copy,
изменяют его applies/advance/speed и проверяют назначение с пустым либо уже
созданным Actor. Назначение получает новый Actor с applies/advance1,speed1;
настройки source Actor остаются0/0/0,5. Прежний target Actor, если был, удаляется.
Оба Projectile/Actor затем удалены, registry пуст, manager удалён. Следовательно,
member20 не является глубокой копией runtime source Actor. PS2 статический
путь также вызывает новый constructor; совпадение всех его runtime defaults
на PS2 этим PC опытом не доказывается.

## Менеджеры снарядов

Двенадцать классов: базовый `wxProjectileManager` и варианты
`wxBacoProjectileManager`, `wxBloomProjectileManager`,
`wxGenericProjectileManager`, `wxGolemProjectileManager`,
`wxGoopMonsterProjectileManager`, `wxIceGargoyleProjectileManager`,
`wxKnutProjectileManager`, `wxMosquitoProjectileManager`,
`wxSpiderProjectileManager`, `wxTrollProjectileManager`, `wxYetiProjectileManager`.
Изученный общий участок управляет записями эффектов/снарядов; индивидуальные
команды стрельбы и выбор траектории ещё открыты.

Все11 новых PC factory/RTTI/default clone/two deletes прошли:55 class operations.
Baco pilot0,975 с, оставшиеся10 —7,057 с тремя workers. У базового manager
используется прежний успешный entity-direct run2, без повторного credit за его
пять class operations. Общая база имеет18 slots, PC6F7008/PS249AFD0,
allocation464/480. Все11 PS2 собственных constructors вызывают base2C6720,
который вызывает wxEntity286CA0. Размеры потомков PC/PS2 различаются на16,20
или28 байт; универсальный сдвиг layout не предполагается.

### Общие методы:35 пар

[Probe](../../research/probe_projectile_manager_common.py),9,201 с:
8 Notify,3 copy,10 registration prefixes,14 Tick cases.

Notify5061E0/2C6590: code1C вызывает virtual16, code1E —virtual17,
code273F — собственный обработчик добавления506100/2C5120. Соседние проверенные
коды возвращаются без изменения records. Callback остановлен у оригинального
consumer, а не заменён успешным возвратом.

Copy505F10/2C6290 после wxEntity переносит14 own words:
PC128,124,134,138,13C,140,144,148,150,154,158,160,164,168;
PS2 соответствующие offsets равны PC+C. PC inherited copy также исполнен;
PS2 начинается после объявленной inherited success и заканчивается у clone
manager104F00 с исходным hash7DB63B02. Проверены zero, различимые значения
и high-bit/NaN bit payloads; всё прочее в manager сохранено.

Pool расположен PC+170 /PS2+17C: **четыре группы, stride20 байт, пять pointers
в группе**. Обработчик code273F ищет пустое место только среди первых **четырёх**
pointers выбранной группы. Первые свободные0,1,3 проверены в группах0/3;
если первые четыре заняты, он возвращается даже при пустом пятом.
Prefixes останавливаются перед реальным выделением12 байт. Это локальная
граница данного producer, не утверждение о недоступности пятого места всем
остальным игровым путям. Выходящий за диапазон group не запускался.

Tick PC5068A0→506650 /PS22C6370 использует оригинальный GameTimer pause query
5968F0/28F020: byte40 ненулевой даёт false. При отсутствии паузы return true;
собственный enabled PC1C0/PS21CC разрешает обход всех4×5 pointers. Record
содержит uint32 deadline+0, active byte+4, payload pointer+8.

При active и **deadline < now** по unsigned сравнению active/deadline сначала
обнуляются, затем payload+24 Node получает Enable(0,1). При равенстве либо
deadline>now вызывается собственный update5063D0/2C57C0 с group/slot.
Проверены pause1/255, disabled, пустой/inactive pool, raw active255,
равенство, unsigned extremes и group3/slot4. Consumer callbacks не исполнены
в prefix случаях; Node и payload до них неизменны. Полный expiration cleanup
и непустой update ещё открыты. PS2 исходные saved-register SQ/LQ обходятся
явно объявленными prefix boundaries; это не полный R5900 runtime.

## Учёт, исправления стенда и открытое

Базовый Projectile PC10→35/PS215→30; manager PC20→45/PS215→40;
20 новых потомков20/15. Всего44 assessments,20 впервые оценённых классов на
каждой платформе. Никакой класс не объявлен полностью закрытым.

В первичных scout labels member был преждевременно назван Collision;
проверка getter766480 установила spActor. Метод5A1600 управляет playback
capacity, а не collision space. Старые окна сохранены с явной поправкой
в контракте; ошибочное имя не внесено как game class. Один micro опыт manager
повторил уже известный protected cap; успешное прежнее evidence переиспользовано.
Первое Copy prefix окно пропустило последние JAL/delay words2C367C/2C3680;
guard остановил опыт, второе окно включает эти оригинальные восемь байт.
Неудачный run и source сохранены; код игры не изменялся.

Открыты траектории, попадания/урон, source/scene initialization, менеджерские
команды стрельбы, полное непустое обновление pool, last-release варианты Copy,
PS2 полные lifetimes и соответствующие исключения allocation failure.
UI/tools и выпуск приложения не менялись.
