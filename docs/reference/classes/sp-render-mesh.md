# spRenderMesh

Описание отдельных известных частей класса. Наличие карточки не означает полного восстановления всех методов.

Общие исходники: [spDXMesh](../../../Sparkplug/Code/SparkplugDX/spDXMesh.h), [spMesh](../../../Sparkplug/Code/Sparkplug/spMesh.h), [spPS2Mesh](../../../Sparkplug/Code/Sparkplug/spPS2Mesh.h), [spRenderMesh](../../../Sparkplug/Code/Sparkplug/spRenderMesh.h).

Статус: класс, прямой базовый тип, обе таблицы virtual-функций и точный размер
`0x50` подтверждены на PC и PS2. Класс не добавляет данных к `spMesh` и не
создаётся через RTTI factory; он задаёт общую точку расширения для
`spDXMesh` и `spPS2Mesh`.

| Факт | PC | PS2 |
| --- | ---: | ---: |
| Class ID / base | `0x67974A9C / spMesh` | то же |
| Null clone | наследуется как `0x004A1BF0` | `0x0015C400` |
| Размер | `0x50` | `0x50` |

Имя и граф наследования извлечены из штатных RTTI-регистраций обеих версий.

## Почему размер доказан

PS2-конструктор сначала вызывает `spMesh` `0x00159EF0`, затем только заменяет
primary vptr по `+0x00` и secondary vptr по `+0x14`. Он не пишет ни одного
нового поля. Concrete `spPS2Mesh` factory `0x001EF6E0` выделяет `0x58` байт,
вызывает этот конструктор и начинает собственные поля ровно с `+0x50` и
`+0x54`.

На PC primary vtable отличается от `spMesh` только deleting destructor и RTTI
getter. Secondary vtable используется та же, что у `spMesh`. Видимый хвост
destructor восстанавливает secondary vtable `0x006EFFE4` и передаёт управление
деструктору `spMesh`. Дополнительно `spDXMesh` начинает платформенный tail с
`+0x50`, что независимо фиксирует ту же границу.

## Восстановленный контракт

Portable `spRenderMesh` наследует весь bounds/resource-контракт `spMesh`,
регистрирует точные ID и base, но оставляет factory пустым и не переопределяет
унаследованный null-clone. Никаких придуманных render-полей в общий класс не
добавлено: D3D и PS2 storage принадлежат только производным типам.

Открыто: исходный header/TU, имена virtual-методов secondary interface и
полный PC constructor body за защищённым factory производного `spDXMesh`.
