# spPS2Helper

Описание отдельных известных частей класса. Наличие карточки не означает полного восстановления всех методов.

Общие исходники: [spPS2Helper](../../../Sparkplug/Code/SparkBasePS2/spPS2Helper.h).

## Идентичность

Registration initializer `0x00484C30` передаёт общей функции регистрации
`0x001115B0` следующие literal values:

| Свойство | Значение |
| --- | ---: |
| class name | `spPS2Helper` @ `0x0045B490` |
| class ID | `0x7E3C519B` |
| base ID | `spBaseObject / 0x415352A1` |
| singleton global | `0x0049F840` (`GP-0x4930`) |
| native size | `0x11C` |

## Layout и lifetime

Factory и clone прямо вызывают `operator new(0x11C)`. Конструктор
`0x001E97A0` вызывает `spBaseObject`, ставит второй polymorphic support vptr,
публикует полный объект в global, а затем инициализирует поля:

| Offset | Size | Роль |
| ---: | ---: | --- |
| `+0x00` | `0x10` | `spBaseObject` |
| `+0x10` | 4 | singleton-support subobject |
| `+0x14` | 4 | path/backend mode; default `0` |
| `+0x18` | 4 | значение, записываемое `0x001E93B0`; роль не названа |
| `+0x1C` | `0x100` | inline C-string prefix; default `host0:` |

Deleting destructor `0x001E95B0`, support destructor `0x001E9640` и thunk
`0x001E9840` сбрасывают singleton global. Clone `0x001E9690` создаёт новый
default object и вызывает inherited пустой `spBaseObject` copy slot. Поэтому
mode, `field18` и prefix исходника не копируются.

У класса нет дополнительных primary virtual slots: после стандартных
destructor/notification/clone/copy/RTTI/type-check slots начинается отдельная
support-vtable. Два полезных path-метода вызываются напрямую.

## `sub_001E91B0`: полный алгоритм пути

Сигнатура по 15 call sites:

```text
void sub_001E91B0(this, const char* input, char* output)
```

### Уже квалифицированный путь

- строка, начинающаяся с `\` и уже содержащая `;1`, копируется;
- строка, начинающаяся с `host0:`, копируется;
- если в такой строке встречается backslash, весь результат дополнительно
  переводится в uppercase; без backslash регистр сохраняется.

Последняя деталь выглядит необычно, но следует из точного control flow:
`strstr(input, "\\")` управляет вызовом uppercase helper `0x0040C548`.

### Mode `0` или `2`: host-style

К inline prefix добавляется input, затем каждый `\` заменяется на `/`.
Default пример:

```text
data\models\flora.smo -> host0:data/models/flora.smo
```

Начальный `./` здесь не удаляется.

### Остальные mode: disc-style

Результат начинается с `\`. Только начальные `./` и `.\` удаляются, затем
добавляется input и ISO-9660 suffix `;1`. Строка переводится в uppercase, а
`/` заменяется на `\`:

```text
./data/models/flora.smo -> \DATA\MODELS\FLORA.SMO;1
```

Setter `0x001E93A0` — одна инструкция, записывающая аргумент в `+0x14`.
Файловые call sites временно переключают helper в host mode и затем
восстанавливают старое значение; bootstrap выбирает `0` или `1` по способу
запуска.

## `sub_001E93B0`: граница системного reset

Метод получает четыре обычных аргумента и пятый в `$t0`. Call sites
`0x003E4AD0` и `0x003E4C64` передают соответственно наборы `1,1,<path>,1` и
`0,0,<path>,0`. Метод строит путь к `IOPRP300.IMG` из одного из literal roots:

- `host0:c:/usr/local/sce/iop/modules/`;
- `cdrom0:\modules\`;
- inline prefix плюс `c:/usr/local/sce/iop/modules/`;
- либо caller-supplied root.

## Связи

- `spPCKManager` возвращает физическое имя backing PCK; на PS2 оно затем
  проходит через `spPS2Helper::sub_001E91B0`.
- `spPS2FileStream` использует тот же helper для обычных файлов и временно
  меняет mode при отдельных memory-loading paths.
- Следующий registration initializer `0x00484C70` относится уже к
  `spPS2IOPModuleManager`, а функция `0x001E9850` является его getter. Это
  подтверждает верхнюю границу кода `spPS2Helper` по `0x001E9844`.

## Открыто

1. Original source/header path и имена двух прямых методов/полей.
2. Точная роль `+0x18` и original enum значений mode.
3. Надёжное сопоставление всех system calls внутри `0x001E93B0` с Sony SDK
   symbols; без этого аппаратный reset не реконструируется.
4. Есть ли отдельный setter inline prefix либо его меняет только внешний код.
