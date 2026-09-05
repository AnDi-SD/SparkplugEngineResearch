# `spRenderMesh`: абстрактная граница платформенного меша

Статус: класс, прямой базовый тип, обе таблицы virtual-функций и точный размер
`0x50` подтверждены на PC и PS2. Класс не добавляет данных к `spMesh` и не
создаётся через RTTI factory; он задаёт общую точку расширения для
`spDXMesh` и `spPS2Mesh`.

| Факт | PC | PS2 |
|---|---:|---:|
| Class ID / base | `0x67974A9C / spMesh` | то же |
| Registration / initializer | `0x00763B40 / 0x006D5190` | `0x004A8E90 / 0x00482660` |
| Constructor | скрыт внутри производного PC factory | `0x0015C3C0` |
| Destructor | protected entry `0x004B1620`, deleting `0x004B1640` | deleting `0x0015C350` |
| Registration getter | `0x004B1610` | `0x0015C340` |
| Null clone | наследуется как `0x004A1BF0` | `0x0015C400` |
| Primary / secondary vtable | `0x006EFFEC / 0x006EFFE4` | `0x0048E2D0 / 0x0048E2F4` |
| Размер | `0x50` | `0x50` |

Оригинальный путь исходника не найден, поэтому
`Code/Sparkplug/spRenderMesh.*` остаётся inferred. Имя и граф наследования
извлечены из штатных RTTI-регистраций обеих версий.

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

`research/inspect_render_mesh.py` проверяет SHA двух исполняемых файлов,
registration thunks, доступные тела, vtables, вызов PS2-конструктора `spMesh`
и начало полей `spPS2Mesh`. CTest сверяет идентичность и оба ABI-layout.

Concrete PS2 leaf теперь разобран отдельно в карточке
[`spPS2Mesh`](native-class-sp-ps2-mesh.md): подтверждены exact `0x58`, owned
`spPS2MeshData*` по `+0x50`, backend helper по `+0x54`, conversion/attach/release,
перенос primitive/vertex counts и непосредственный renderer consumer.

Открыто: исходный header/TU, имена virtual-методов secondary interface и
полный PC constructor body за защищённым factory производного `spDXMesh`.
