# spPlatformSpecificMeshData

Описание отдельных известных частей класса. Наличие карточки не означает полного восстановления всех методов.

Общие исходники: [spDXMeshData](../../../Sparkplug/Code/Sparkplug/spDXMeshData.h), [spPS2MeshData](../../../Sparkplug/Code/Sparkplug/spPS2MeshData.h), [spPlatformSpecificMeshData](../../../Sparkplug/Code/Sparkplug/spPlatformSpecificMeshData.h).

| Platform | PC | PS2 |
| --- | ---: | ---: |
| Class ID / base | `0x71BE79C5 / spNamedObject` | same |
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
