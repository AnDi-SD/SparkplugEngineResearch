# `spPS2Helper`: PS2 path/backend helper

Статус: class identity, RTTI, exact layout, singleton lifetime, clone и полный
алгоритм преобразования путей восстановлены. Платформенная операция
`0x001E93B0` очерчена до аргументов и системных ветвей, но не переносится на
host: исходные имена метода и вызываемых Sony SDK entry points отсутствуют.

Контрольный файл —
`local-data/Winx Club the game PS2/SLES_532.19`, SHA-256
`198313352DBF4FA26FF8C5D509F6783FC32F9B504A627E416323C5FFBBFFE8FE`,
`GP = 0x004A4170`.

## Идентичность

Registration initializer `0x00484C30` передаёт общей функции регистрации
`0x001115B0` следующие literal values:

| Свойство | Значение |
|---|---:|
| class name | `spPS2Helper` @ `0x0045B490` |
| class ID | `0x7E3C519B` |
| base ID | `spBaseObject / 0x415352A1` |
| registration | `0x004B6E40` |
| registration getter | `0x001E91A0` |
| factory | `0x001E97A0` |
| primary vtable | `0x00491080` |
| support vtable | `0x004910A4` |
| singleton global | `0x0049F840` (`GP-0x4930`) |
| native size | `0x11C` |

PC executable не содержит ни class ID, ни имя `spPS2Helper`. Прежний
PC-кандидат `0x004C33C0` также окончательно исключён, но более точная повторная
проверка исправила его владельца: virtual registration getter `0x004C33A0`
возвращает `0x00764770`, регистрацию соседнего `spPCErrorManager`. Адрес
является factory именно этого error manager, а строка `spPCApp` лишь лежит
перед его vtable в `.rdata`. Поэтому PS2-класс не объединяется с
несуществующим общим/PC helper API.

## Layout и lifetime

Factory и clone прямо вызывают `operator new(0x11C)`. Конструктор
`0x001E97A0` вызывает `spBaseObject`, ставит второй polymorphic support vptr,
публикует полный объект в global, а затем инициализирует поля:

| Offset | Size | Роль |
|---:|---:|---|
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

Native output не имеет аргумента capacity и заполняется `strcpy/strcat`.
Переносимая реконструкция возвращает `std::string`, не воспроизводя переполнение.

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

Далее он вызывает платформенные shutdown/reset/synchronization entry points,
в отдельных ветвях ожидает их завершения и записывает третий аргумент в
`+0x18`. Имена SDK-функций и исходные enum names ещё не доказаны. В
переносимом классе этот hardware-affecting метод намеренно не имитируется.

## Связи

- `spPCKManager` возвращает физическое имя backing PCK; на PS2 оно затем
  проходит через `spPS2Helper::sub_001E91B0`.
- `spPS2FileStream` использует тот же helper для обычных файлов и временно
  меняет mode при отдельных memory-loading paths.
- Следующий registration initializer `0x00484C70` относится уже к
  `spPS2IOPModuleManager`, а функция `0x001E9850` является его getter. Это
  подтверждает верхнюю границу кода `spPS2Helper` по `0x001E9844`.

## Реконструкция и тесты

- inferred `Sparkplug/Code/SparkBasePS2/spPS2Helper.h/.cpp`;
- exact PS2 layout и адреса в `Sparkplug/Analysis/PS2/SparkBaseAbi.h`;
- тесты default state, singleton, RTTI, clone, host/disc modes,
  qualified paths и обе dot-prefix формы.

## Открыто

1. Original source/header path и имена двух прямых методов/полей.
2. Точная роль `+0x18` и original enum значений mode.
3. Надёжное сопоставление всех system calls внутри `0x001E93B0` с Sony SDK
   symbols; без этого аппаратный reset не реконструируется.
4. Есть ли отдельный setter inline prefix либо его меняет только внешний код.
