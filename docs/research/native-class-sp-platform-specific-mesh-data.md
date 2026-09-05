# `spPlatformSpecificMeshData`: platform mesh boundary

Статус: class identity, direct base, factory, clone, vtable и отсутствие
собственных полей подтверждены на PC и PS2. Платформенные descendants
`spDXMeshData` и `spPS2MeshData` установлены. Первый уже разобран отдельно до
exact PS2 layout и подтверждённого PC prefix; второй остаётся следующим leaf.

| Platform | PC | PS2 |
|---|---:|---:|
| SHA-256 | `3F022480BF55045DA4BF692E4BC8862ED38FC024E8A964A558FBDFDF646DFC4F` | `198313352DBF4FA26FF8C5D509F6783FC32F9B504A627E416323C5FFBBFFE8FE` |
| Class ID / base | `0x71BE79C5 / spNamedObject` | same |
| Registration / initializer | `0x00762A48 / 0x006D48F0` | `0x004A8FB0 / 0x00482720` |
| Factory / constructor | `0x004920F0 /` protected body | `0x0015D4F0 / 0x0015D3E0` |
| Destructor / clone | `0x004920E0`, deleting `0x004921B0` / `0x00492160` | deleting `0x0015D380` / `0x0015D420` |
| Getter / vtable | `0x004920D0 / 0x006ECB00` | `0x0015D370 / 0x0048E370` |
| Exact size | `0x14` | `0x14` |

## Назначение границы

Это не производный `spMeshData`. RTTI обоих executable ставит класс напрямую
под `spNamedObject`. Его назначение — именованная platform-specific форма,
которую serializers присоединяют к cross-platform `spMeshData`:

```text
spNamedObject
  -> spPlatformSpecificMeshData
       -> spDXMeshData   (0x3178114C)
       -> spPS2MeshData  (0x737D740F)
```

Оба leaf-типа зарегистрированы в PC executable. PS2 executable также содержит
обе строки и регистрирует `spPS2MeshData`; присутствие DX serializer там
подтверждает, что эта иерархия обслуживает преобразование данных, а не только
активный GPU backend.

## Layout и поведение

PS2 factory выделяет ровно `0x14` байт. Constructor вызывает
`spNamedObject` и меняет только vptr. На PC собственная factory защищена
`.rld`, но первый доказанный `spDXMeshData` field записывается по `+0x14`, а
base destructor/clone/vtable не обращаются к дополнительному состоянию. Таким
образом layout целиком совпадает с `spNamedObject` на обеих платформах.

PC vtable содержит семь обычных SparkBase slots. PS2 содержит те же девять
ABI-slots с двумя platform stubs. Clone на PC `0x00492160` и PS2 `0x0015D420`
создаёт новый объект, регистрирует пару в `spCloneManager` и вызывает inherited
name-copy slot. Никакого mesh payload на этом уровне нет.

Portable-класс поэтому реализован как concrete storage-free boundary с
native RTTI factory и копированием имени. Это не заменяет будущие реализации
leaf-классов и не вводит общего fictitious vertex API.

## Открыто

- original header и translation unit не сохранились строками;
- исходные имена PS2 vtable stubs неизвестны;
- `spDXMeshData` разобран в отдельной
  [`native-class-sp-dx-mesh-data.md`](native-class-sp-dx-mesh-data.md), но его
  PC `sizeof` и platform tail ещё открыты;
- `spPS2MeshData` разобран в отдельной
  [`native-class-sp-ps2-mesh-data.md`](native-class-sp-ps2-mesh-data.md): PS2
  exact `0x100`, PC observed extent `0x100`, constructor tables, packet
  ownership, buffer normalization и serializer connection подтверждены.
