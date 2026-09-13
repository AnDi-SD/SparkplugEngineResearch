# spSerializerHook

Описание отдельных известных частей класса. Наличие карточки не означает полного восстановления всех методов.

Общие исходники: [spSerializerHook](../../../Sparkplug/Code/Sparkplug/spSerializerHook.h).

## Область доказательств

Original translation unit и header общего/PS2 cluster не найдены. Поэтому `Sparkplug/Code/Sparkplug/spSerializerHook.*` — явно inferred path, а имя последнего virtual метода сохраняется как `vfunc_24`.

## Identity и наследование

| Класс | Class ID | Direct base | PC registration/init | PS2 registration/init |
| --- | ---: | --- | --- | --- |
| `spSerializerHook` | `0x18092F8D` | `spBaseObject` | `0x00763D20 / 0x006D5280` | `0x004A9D90 / 0x00483010` |
| `spDXSerializerHook` | `0x0D832A30` | `spSerializerHook` | `0x007631B0 / 0x006D4D10` | отсутствует |
| `spPS2SerializerHook` | `0x1C0E0F30` | `spSerializerHook` | отсутствует | `0x004B83B0 / 0x00485730` |

PS2 common base и leaf имеют exact размер `0x10`, то есть storage сверх
`spBaseObject` нет. Base registration не содержит factory, clone
`0x0017E730` возвращает null, а последний vtable slot `+0x24` в таблице
`0x0048EF30` равен null. Это согласуется с abstract interface.

`spPS2SerializerHook` concrete: factory `0x00209070`, constructor helper
`0x00209010`, clone `0x00208F40`, destructor `0x00208EE0`, RTTI getter
`0x00208E60`, vtable `0x00491BB0`. Clone создаёт чистый leaf без состояния.

PC `spDXSerializerHook` имеет vtable `0x006EF3C8`, destructor `0x004AA370`,
deleting destructor `0x004AA410`, blank clone `0x004AA490` и RTTI getter
`0x004AA3A0`. Destructor доказывает старый MSVC list state по
`+0x10..+0x1B`; из-за защищённой allocation точки это пока observed prefix
`0x1C`, а не объявленный exact `sizeof`.

Последний PC slot находится по `+0x1C`, тогда как PS2 ABI помещает его по
`+0x24`. Имя `vfunc_24` в portable interface сохраняет уже принятую PS2
координату и не означает совпадение сырых vtable offsets платформ.

## Вызов из generic load-path

PS2 `spSerializerManager` generic loader `0x00182640` после чтения FAT и
установки logical stream origin:

1. создаёт `spPS2SerializerHook` через `0x00209010`;
2. вызывает slot `+0x24` с `(manager FAT helper, source stream)`;
3. уничтожает временный hook;
4. продолжает materialization FAT entries.

Следовательно, hook является частью native import path, но не сериализатором
конкретного типа объекта.

## Exact PS2 slot `0x00208E70`

Функция не читает `this`, FAT или stream. Она:

Portable `spPS2SerializerHook::vfunc_24` сохраняет этот эффект. Для случая без
manager-а он создаёт process-lifetime fallback, потому что native allocation
также не освобождается самим hook. Уже существующий manager, FAT и stream не
изменяются.

## PC DX hook

PC factory entry `0x004AA430` состоит из exact thunk
`FF 25 98 2D 3B 01` к slot `0x013B2D98`. Та же форма присутствует во всех
доступных копиях PC executable. Это скрывает factory/constructor, но не другие
методы класса.

Slot `0x004AAB80` вызывает основную функцию `0x004AA870` только при platform
bit2; bit4 без bit2 недостаточен, Common1 пропускает body. Она работает до
обычной materialization FAT и выполняет следующее:

Общий `0x2C` D3D-контейнер теперь точно идентифицирован как
`spDXMeshCombiner`: доказаны layout, ownership двух DX wrappers, lock cursors,
commit и unlock. Поэтому portable hook реализует metadata reader и batch plan,
а отдельный portable combiner — проверяемое GPU-storage state без Direct3D.
`HasCompleteNativeMaterializationForAnalysis() == false` остаётся корректным:
сам hook ещё не выполняет настоящий serializer dispatch второго прохода.

### `ReadDXMeshDataInfo`

```text
u32 FVF
u32 vertexCount
u32 vertexDataSize
u32 indexDataSize
u8  indicesAre32Bit
```

После чтения функция возвращается к началу payload и вызывает общий
`SkipData`; остальные fields сразу пропускаются тем же способом. Terminator
завершает объект. Это связывает platform hook с общим field codec без
отдельного придуманного parser-а.

## Открыто

- original header общего interface и имя последнего virtual метода;
- exact PC `sizeof`/constructor за protected factory (prefix `0x1C` доказан);
- полный portable serializer dispatch второго прохода и rollback ошибок;
- был ли пустой PS2 branch результатом build flags или удалённой логики;
- точная ownership-модель lazy manager allocation на завершении процесса.
