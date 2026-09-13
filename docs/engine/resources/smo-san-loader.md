# PC: общий загрузчик SMO/SAN, FAT и фабрика классов

## Целый вызов общего PC loader-а

```text
FFPS header /422260
 -> resource index /466B90 -> RTTI contains /4143F0 ->4423F0
 -> file index /465CD0 ->13BCA90
 -> stream origin += dataOffset
 -> spDXSerializerHook /4AA430, v1C=4AAB80 (platform1: gate skips4AA870)
 -> materialization /422940
    -> serializer lookup /4224F0 ->42C9F0
    -> object header /467550 -> RTTI create /414420 ->41A090
    -> spAnimationSerializer secondary v8 /43ECC0
 -> FAT clear /466760,466870
```

## Реестр сериализаторов

`422D90(classID, serializer, platformMask, operationMask)` добавляет node18 в
конец списка. `4224F0 ->42C9F0` выбирает первый совпавший ID с ненулевым
пересечением обеих масок, включая старший bit32. Null serializer у первого
совпадения возвращает ноль, а не запускает поиск следующей записи. `4228A0`
уничтожает каждый уникальный serializer один раз, группируя aliases по первому
появлению. Portable `RegisterForAnalysis` оставляет явный безопасный запрет
null registrations; это отличие host API, не найденный native guard.

## PC FAT отличается от PS2

Allocation PC = **0x58** (`push58` по `13D6E7F` и bounded manager scout до входа
в защищённый ctor), PS2 = **0x64**. Имя C++ helper-а неизвестно; доказан путь
`Z:\Sparkplug\Code\Sparkplug\spResourceFATSerializer.cpp`.

| PC offset | Наблюдаемая роль |
| ---: | --- |
| `10` | next save resource ID |
| `14 /20 /2C` | три 12-байтных map: file ID, resource ID, object pointer |
| `38` | ordered file list, 12 байт |
| `44` | неизвестно; не объявлять cursor по симметрии |
| `48 /54` | ordered resource list и current node |

## RTTI и различие физических / engine типов

Initializer `spAnimation` начинается по **6D1C10**; **6D1C30 — callsite** внутри
него. Статически задаёт class56EE563A, parent75DE50, factory41A090. Engine RTTI:
Animation ->Controller ->SubController ->BaseObject. NamedObject44DE07FD на
анимации даёт false, несмотря на физический named-object prefix. Подмена engine
RTTI физическим наследованием изменила бы cache/name ветви загрузчика.
