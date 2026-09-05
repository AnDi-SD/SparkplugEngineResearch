# `wxPS2App`: PS2 bootstrap игрового слоя

Статус: registration, exact `0x48` allocation, обе vtable, factory/clone/
destructor, build title и три lifecycle overrides восстановлены. Hardware и
global-manager side effects очерчены, но не выполняются portable-кодом.

Контрольный файл — `local-data/Winx Club the game PS2/SLES_532.19`, SHA-256
`198313352DBF4FA26FF8C5D509F6783FC32F9B504A627E416323C5FFBBFFE8FE`,
`GP = 0x004A4170`. Exact `.cpp` path в ELF отсутствует; расположение
`Winx/Code/PS2` inferred.

## Identity, vtable и layout

Initializer `0x00487B10` регистрирует class ID `0x36973698`, base
`spPS2App / 0x354B1350`, имя `wxPS2App` @ `0x00470978`, registration
`0x004C4DF0`, factory `0x003E5C00`, null property callback.

Primary vptr `0x00495130` содержит deleting destructor `0x003E5AB0`,
notification `0x003E4FF0`, clone `0x003E5B20`, inherited copy
`0x00105DC0`, registration getter `0x003E4FE0` и стандартные type checks.
Support vptr `0x00495154` содержит thunk `0x003E5C70`, exact-title getter
`0x003E50A0`, initialize `0x003E53D0`, update `0x003E50B0`, shutdown
`0x003E5310` и inherited run loop `0x001E7660`.

Обе concrete creation paths `0x003E5040` и factory `0x003E5C00` вызывают
`operator new(0x48)`, затем `spPS2App` constructor и заменяют два vptr.
Layout:

```text
+0x00  spPS2App                         0x20
+0x20  manager/interface pointer        0x04
+0x24  manager/interface pointer        0x04
+0x28  manager/interface pointer        0x04
+0x2c  manager/interface pointer        0x04
+0x30  без прямых wxPS2App accesses     0x18
```

Initialize присваивает первым четырём полям адрес `global service +0x50`.
Названия полей не известны; одинаковое значение не доказывает одинаковую
семантику. По ограниченному скану тела класса offsets `+0x30..+0x47` напрямую
не читаются и не пишутся.

## Lifecycle

Title getter возвращает literal `Winx Club (PS2) - Build 0.00.08`.

Update `0x003E50B0` не обращается к `this`: берёт delta time из global engine
state, обновляет глобальные game/platform systems, выполняет условный display
path и возвращает true. Shutdown `0x003E5310` последовательно вызывает четыре
global teardown paths, затем `spPS2App::Shutdown` и сбрасывает GP-relative
byte. Initialize `0x003E53D0` — большой bootstrap: связывает четыре interface
fields, создаёт services по GP slots, вызывает base initialize, задаёт video
configuration, загружает named resources/fonts и соединяет engine core с game
managers. Все error exits сходятся в false; успешный хвост возвращает true.

Native clone создаёт новый default `0x48` object, регистрирует его в clone
manager и вызывает inherited `spNamedObject` copy. Portable реализация
сохраняет concrete type и shared name, но не переносит внешние manager links.

## Открыто

1. Exact source path и original names четырёх interface fields.
2. Почему allocation содержит неиспользуемый этим TU tail `+0x30..+0x47`.
3. Точные классы всех GP-relative globals и полный порядок ownership.
4. SDK calls/video configuration и условия каждого initialize error exit.
