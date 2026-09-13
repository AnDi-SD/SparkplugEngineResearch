# Снаряды и менеджеры: два семейства PC/PS2

## Объекты снарядов

Десять классов: `wxProjectile`, `wxFetidBreathProjectile`,
`wxGolemLitProjectile`, `wxGolemShockwaveProjectile`, `wxPiercingWindProjectile`,
`wxStarsProjectile`, `wxStingerProjectile`, `wxStompProjectile`, `wxTidalFlame`,
`wxWebSpitProjectile`. Это отдельные варианты снаряда; траектории, урон и
полный игровой эффект каждого пока не закрыты. Их размеры совпадают между
PC/PS2: соответственно236,272,268,328,476,552,248,340,424,256 байт.

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
[spController](../../reference/classes/sp-controller.md),
[spActor](../../reference/classes/sp-actor.md),
[spAnimationManager](../../reference/classes/sp-animation-manager.md).

### Copy: шесть пар и два PC опыта

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

### Общие методы:35 пар

Tick PC5068A0→506650 /PS22C6370 использует оригинальный GameTimer pause query
5968F0/28F020: byte40 ненулевой даёт false. При отсутствии паузы return true;
собственный enabled PC1C0/PS21CC разрешает обход всех4×5 pointers. Record
содержит uint32 deadline+0, active byte+4, payload pointer+8.

## Учёт, исправления стенда и открытое

Базовый Projectile PC10→35/PS215→30; manager PC20→45/PS215→40;
20 новых потомков20/15. Всего44 assessments,20 впервые оценённых классов на
каждой платформе. Никакой класс не объявлен полностью закрытым.

Открыты траектории, попадания/урон, source/scene initialization, менеджерские
команды стрельбы, полное непустое обновление pool, last-release варианты Copy,
PS2 полные lifetimes и соответствующие исключения allocation failure.
UI/tools и выпуск приложения не менялись.
