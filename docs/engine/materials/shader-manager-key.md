# PC shader manager and renderer key

## Manager lifetime and cache

Actual protected4C9680 factory succeeds under unchanged limits: size0x50,
primary6F2DE4 and secondary+10=6F2DE0. Empty vectors18/1C/20 and28/2C/30,
map root38/count3C, NULL template40, pair-key map root48/count4C.
Allocator words14/24/34/44 are untouched, not fabricated pointer defaults.
Primary slots:4C9660,5B7A00,4C96E0,40ECE0,4C9480,408350,408370,
4C8980,4C97E0,4C8F10. The next words are a string, not more virtual slots.

Zero returnsNULL without even looking at the cache, including when an entry for that key exists. Otherwise4C80F0 finds the pair. Node+14 is the returned shader pointer. Cache miss enters source generation/compilation; it is NOT a normal NULL hit. The source `SelectCachedForAnalysis` explicitly returns completed=false on such a miss. No internal compilation function is replaced by fake success.

Clone4C96E0 registers an actual clone and calls40ECE0: new **empty** manager,
not a copy of cache/template/vectors.4C9370 destroys template40 if present,
direct-deletes each non-NULL cache value, destroys nodes/root, then4B0740
destroys both owned shader vectors and base map.4B0740 unconditionally
clears763024, even if its value no longer equals this manager. Portable
source deliberately has no process-global renderer/manager singleton;
that lifetime side effect is documented, not claimed as implemented.
Populated first/second vectors and base name-map ownership remain separate
from the bounded fresh Fixed-template path described below.

## Загрузка при запуске

Общий исходник [spDXShaderManager](../../../Sparkplug/Code/SparkplugDX/spDXShaderManager.cpp)
восстанавливает порядок `4B0890`: поиск `Shaders\\*.vsh`, загрузка каждого
basename через виртуальный per-file метод, сохранение ненулевых shader owners,
закрытие поиска; затем те же действия для `Shaders\\*.psh`. Даже при отсутствии
файлов вызывается `_findclose(-1)`, а метод заканчивается успехом. Порядок файлов
задаёт ОС; исходник его не сортирует. Directory provider описывает CRT-границу,
а per-file provider — отдельный виртуальный метод движка `4C8F10`; это разные
границы, и предоставленный shader ещё не доказывает выполнение его компилятора.

`spPCShaderManager::StartForAnalysis` соединяет этот порядок с настоящим
[RFX loader](rfx-pipeline.md): формирует `Shaders\\Fixed.rfx`, читает документ,
присваивает владельца шаблона и только затем вызывает его инициализацию.
Нулевой результат загрузки завершает startup со значением false; ненулевой
шаблон даёт native true независимо от AL инициализатора. XML-library provider
передаёт decoded events восстановленным callback-методам. `completed` отдельно
показывает, удалось ли выполнить этот безопасный аналитический путь: отсутствие
декодированных событий не превращается в придуманный native false.

Интерфейс рассчитан на свежий manager. Повторный native startup перезаписывает
указатель шаблона без освобождения старого; переносимая реализация отказывается
от повторного вызова до изменения состояния. Это явное ограничение host, а не
исправление поведения оригинала. Аналогично ограничены пути, не помещающиеся
в исходный 256-byte buffer. Базовая name-map и процессное владение singleton
остаются отдельными обязательствами. Per-file путь представлен следующим
общим методом; caller scan может передать его как concrete loader.

## Загрузка одного standalone shader

[spPCShaderManager_Standalone.cpp](../../../Sparkplug/Code/SparkplugPC/spPCShaderManager_Standalone.cpp)
содержит `LoadStandaloneFileForAnalysis`, соответствующий `4C8F10`.
К имени из directory enumeration добавляется `<root>Shaders\`; общий
`spParser::ReadFileTextForAnalysis` читает файл и добавляет четыре нулевых
байта. Внешний D3DX assembler получает source до первого NUL, его длину,
нулевые macros/include/flags и два output slots для code/error buffers.
Общий compiler-active byte устанавливается перед этим вызовом и очищается
сразу после него, в том числе при отсутствии code buffer.

HRESULT не служит условием успеха. При ненулевом code buffer запрашивается
его размер, CRT `_splitpath` выделяет имя без пути и расширения, создаётся
`spPCVertexShader` или `spPCPixelShader`. Имя копируется в исходный field14,
затем реальный parser комментариев добавляет параметры. В его безопасном
domain native parser возвращает1. Vertex device Create вызывается всегда;
renderer byteF2F5 разрешает или пропускает только pixel Create.
Обе device-обёртки игнорируют HRESULT,
поэтому shader может существовать с пустым device handle.

В конце сначала освобождается error buffer, если он был, затем code buffer.
Размер code запрошен, но копии в shader48/4C здесь нет: эта пара полей
остаётся пустой. Этим standalone path отличается от generated/RFX compile.
Device handle принадлежит shader и освобождается его destructor.

При отсутствии code native читает текст error buffer без проверки на NULL,
передаёт его диагностике и возвращает NULL shader. Error interface на этой
ветви не освобождается. Аналитический результат сохраняет явные unreleased
wrappers; отсутствующий error buffer обозначается invalid library contract.
Общий error manager выделяет40байт под error из своего фиксированного
data stack и отдельно хранит сообщение с завершающим NUL. Новый error
связывается первым и наследует более высокую severity прежней головы.
В этом пути slot1C печатает один error в консоль с цветом головы очереди,
сохраняя всю цепочку. Lazy modal handler не разрешается; обработка и очистка
очереди здесь не выполняются. Поэтому повторные ошибки накапливаются.
Переносимый manager владеет error objects до `ClearForAnalysis`; console
provider явный, по умолчанию применяется debugger output. Это отличается
от отдельного [modal-handler policy](../../reference/classes/sp-pc-error-manager.md).
Fatal dispatch и переполнение data stack не квалифицированы этим API.
Отказ file Open также возвращает NULL; безопасное RAII-отличие shared text
reader от исходной утечки stream описано у `spParser`.

Границы внешних providers — ОС/file stream, CRT splitter, D3DX buffers и COM.
Это не восстановление SDK assembler и не самостоятельный device backend.
Путь ограничен исходным256-byte target, имя —31байтом; используемый code
buffer требует выровненных word data и ограниченного размера. Эти guards
принадлежат переносимому API, а не проверкам оригинала.

## Комментарии standalone ASM

[spDXAsmShaderParser](../../../Sparkplug/Code/SparkplugDX/spDXAsmShaderParser.cpp)
содержит невиртуальный алгоритм `4CF950`. Он ищет каждый маркер `//`, начиная
следующий поиск через два байта после предыдущего. Текст инструкций здесь
не разбирается: сборку выполняет внешний D3DX assembler. Метод собирает
метаданные из трёх форматов комментариев:

```text
// Name : register cStart (Count registers total)
// Name : registers cStart-cEnd (Count registers total)
// Component Name
```

У диапазона сохраняются `Start` и явный `Count`; `End` не участвует в вычислении
числа строк. Описатели добавляются к существующим через общую shader-операцию,
которая разрешает имя параметра и суммирует count с uint32 wrap, см.
[shader constants](shader-constants.md). Входной component mask каждый раз
обнуляется, затем собирается OR из таблицы Position, RHW, Weight1..4,
BlendIndex, Normal, PointSize, Color, SpecColor, UVW1, UV1..8, Tangent и Binormal.
Сравнение имён чувствительно к регистру; неизвестные имена игнорируются.

Форматы используют `sscanf`: пробел формата допускает ноль или больше
whitespace-символов, а register scanset исключает только двоеточие и обычный
пробел. Поэтому табуляция может стать частью имени. После присваивания всех
полей несовпавший хвост `registers total)` уже не отменяет комментарий.
Первый NUL завершает вход. Переносимый parser ограничивает decimal signed int32
и имя описателя 31 байтом; небезопасная оригинальная запись длинного имени
не воспроизводится. Это разбор аннотаций, не восстановление SDK ASM grammar.

## Renderer key, original4BE310

The bounded prefix executes through original material power getter and actual
manager factory, stopping **before** indirect call4BE495, never resuming it.
Separate no-weight cases execute the whole4BE310→4C8980 and returnNULL.
Native source-generation format strings give real parameter names:

| Key portion | Original meaning and construction |
| --- | --- |
| bits0..3 | BlendWeightCount: highest component bit10/8/4/2 gives4/3/2/1 |
| bits4..7 | texture-coordinate count: highest component bit40000..800 gives8..1 |
| bits8..15 | bHasUVTransform[0..7]: raw state8 atC8B8+24hex*stage has bit2 |
| bits16..19 | ColorMode; construction actually ORs **raw C888<<16**, no mask |
| bits20..23 | LightCount from lightsC190+24, shifted20, no count mask |
| bit24 | bUseSpecular iff selected materialE47C adjusted+14 getter+28 returns >0 |
| second word | OR raw light+C0 types shifted2*i, without masking each type |

Zero/negative/NaN power clear specular; positive subnormal and +infinity set
it. Raw out-of-range color/type inputs preserve overlapping bits exactly.
Source limits explicit light arrays to8; native has no such loop guard.
These are consumer inputs, not a claim that renderer startup permits all
raw combinations. No PC observations are transferred to PS2.
