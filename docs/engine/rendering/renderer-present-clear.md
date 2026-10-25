# PC Clear, Present и жизненный цикл устройства

## Clear4BB980

Secondary `this=complete+18`; arguments flags, ARGB, stencil. Reads the low
flag byte and maps only bits0..2. Calls device complete+C9E8, COM slot+AC,
with rectangle count0, NULL rectangles, `flags&7`, unchanged ARGB, float
depth1.0, unchanged stencil. Returns AL1 even for a failing HRESULT.
All256 low-byte combinations plus100/107/80000000/FFFFFFFF are tested both
with success and failure HRESULTs:260 inputs per native child.

## Present4BB9F0

Complete+C1C0 nonzero returns AL0 without touching the device. Otherwise
COM slot44 receives four NULL arguments. Any HRESULT except88760868
(DEVICELOST), including generic failure, leads to AL1. DEVICELOST calls
COM slot0C (TestCooperativeLevel). Only88760869 (DEVICENOTRESET) follows
the internal reset path; other cooperative results return AL1.

The reset path copies32 bytes complete+1C..3B onto the stack and prepares secondary vtable slot8/+20=4BD810 with a pointer to that copy. The wrapper's one stack argument is not read in this body. A stop before that call establishes only the Present boundary, not a completed reset.

Portable `PresentBeforeResetForAnalysis` returns an explicit incomplete/
reset-required boundary instead of inventing reset behavior. A NULL external
callback is a host guard, not a native behavior claim. Clear and Present APIs
use analysis names and do not imply a complete native renderer layout.

## Создание устройства

[spDXRenderer_Init.cpp](../../../Sparkplug/Code/SparkplugDX/spDXRenderer_Init.cpp)
содержит отдельные операции capability query `4BCC80`, выбора устройства
`4BCCF0`, построения presentation parameters `4BD050` и reset `4BD810`.
Их переносимые carrier-типы не являются native ABI и не создают процессный
singleton. COM provider получает известные входы и изменяемые output slots;
неизвестный необходимый output означает incomplete.

`4BCC80` получает caps для adapter/device type. Pixel shader доступен при
unsigned version token не меньше `0xFFFF0101`, vertex shader — не меньше
`0xFFFE0101`. HRESULT не проверяется. Поэтому ошибка API сама по себе не
разрешает подставить нулевые capabilities: переносимый provider должен
явно предъявить инициализированные выходные данные.

В `4BCCF0` по умолчанию выбирается последний adapter и HAL type1.
Mask `0x4` в configuration word `+14` выбирает adapter0. При доступном vertex
shader используются hardware vertex processing flags `0x40`, иначе software
`0x20`. Mask `0x2` добавляет `MULTITHREADED=0x4`; mask `0x8` выбирает REF type2
и software processing. Затем перечисляются описания adapters. Точное совпадение
17 bytes с `NVIDIA NVPerfHUD` вместе с NUL переопределяет adapter и также
выбирает REF/software; дополнительные flags сохраняются. Mixed processing
`0x80` этот body не выбирает.

`CreateDevice` получает HWND из configuration `+18`, изменяемый блок presentation
parameters и прямой output slot устройства. Старый device предварительно не
освобождается. HRESULT игнорируется, метод возвращает true; при неуспешном API
output может остаться прежним. Это свойство wrapper, а не подтверждение создания
работающего устройства. Переносимая граница ограничивает каждое значение
adapter count числом 256, сохраняя порядок повторных queries оригинала.

## Presentation parameters

Входная configuration содержит восемь raw words. `4549A0` копирует все 32 bytes
в renderer; это copy, а не validator. `4BD050` записывает только известные
слова четырнадцатисловного `D3DPRESENT_PARAMETERS`: обнуление остальных полей
выполняет caller. Width/height копируются, swap effect зависит от low byte
configuration `+10`, auto depth включён, depth format выбирается по `+0C`.

Windowed-ветвь читает текущий display format выбранного adapter. Format `0x29`
даёт false и error code `0x10000001`; обычная ветвь устанавливает backbuffer
format/count и изменяет color-choice word configuration. Fullscreen-ветвь
проверяет пары форматов через `CheckDeviceType`, включая fallback; отказ обеих
пар даёт false и code `0x10000002`. Нулевой color choice становится2. Количество
backbuffers равно `1 + bool(flags & 1)`, presentation interval равен1.
Небезопасный индекс diagnostic format table вне известного домена0..3
переносимый интерфейс отклоняет.

## Порядок reset и ресурсов

`ResetDeviceForAnalysis` выражает следующий порядок `4BD810`:

1. Проверка наличия Direct3D interface; copy configuration и подготовка
   presentation parameters. Native игнорирует false последнего helper.
2. Освобождение persistent index handle, затем release и уничтожение записей
   временных index buffers с освобождением их vector storage.
3. При активном offscreen состоянии — bind ordinary render target NULL через
   secondary slot `+4`. Это отдельный вызов binding, не `EndScene`.
4. Texture virtual release, затем render-target release: ordinary, cube,
   layer. У ordinary/cube release результат не проверяется; отказ layer
   завершает reset до device call.
5. `IDirect3DDevice9::Reset`; HRESULT игнорируется. Reinit ordinary, cube и layer
   останавливается на первом false. Затем выполняется texture virtual reinit.
6. Persistent index создаётся повторно с usage `0x208`, format `0x65`, pool0;
   его wrapper также игнорирует HRESULT. Сбрасываются bindings, borrowed cache
   identities и refcounted отношения; shader cache identities получают
   `0xFFFFFFFF`, восемь light identities —0 и dirty bytes —1.
7. Primary slot `+44` восстанавливает состояния. Его false завершает reset
   неуспехом. Отсутствующая реализация этого engine virtual method даёт
   incomplete/restoreRequired после предыдущих изменений.

Ресурсные cohort-типы задают уже известные owners и virtual leaves.
Переносимый метод сам выполняет manager-порядок и failure gates; high-level
manager не подменён callback, всегда возвращающим true. При этом конкретный
texture backend и обнаружение owners не объявляются
восстановленными. Callback mutation/reentry списка и native uint16 reference
overflow не моделируются. В существующем общем render-target manager остаётся
отдельный host-путь с aggregate reinit; он не используется как точный native
reset-порядок.

## Восстановление кэшей состояний

`RestoreDeviceStateCachesForAnalysis` содержит полный body `4BCF20`, общий
invalidator `454940` и настоящий selector `4BCEC0`, чей защищённый переход ведёт
к `431760`. Данные selector являются общими globals, а не полями отдельного
renderer: dirty byte, 256 cache bytes и таблица 32-bit значений с отдельным count.
При dirty=0 selector возвращает сырой cache byte выбранного индекса, включая
значения больше1. Условный consumer проверяет этот byte на ноль.

В dirty-ветви оригинал перебирает кандидаты0..255, для каждого заново сбрасывает
временный флаг и ищет кандидата в таблице. После всего цикла он записывает
**только запрошенный cache byte**, сбрасывает dirty и возвращает результат
последнего кандидата255. Он не заполняет все256 cache bytes и не использует
запрошенный индекс как искомое значение. Восстановленный исходник сохраняет
этот необычный порядок. Остальные cache bytes остаются входным состоянием.
Переносимый API отклоняет индекс вне0..255 и dirty-таблицу длиннее256 элементов;
это ограничения host-интерфейса, отсутствующие в оригинальном body.

Восстановление кэшей выполняется так:

1. Общий invalidator записывает `0xFFFFFFFF` в12 raw render words и72 raw
   texture words. Material identity `E47C` и fog identity `CA20` также
   инвалидируются.
2. Для render IDs0..255 вызывается selector. При ненулевом byte выполняется
   `GetRenderState` через device slot `+E8` с прямым output slot в кэше.
   Отказ selector либо любой ненулевой HRESULT даёт `0xFFFFFFFF`.
3. Для каждого из8 stages и texture IDs0..63 применяется тот же selector и
   `GetTextureStageState` через slot `+108`. Device-кэш имеет размер8×64;
   его нельзя смешивать с72 raw words общего invalidator.
4. Light-list identity `C190` обнуляется. Через существующий cached writer
   `4B0A90` задаются по порядку состояния `143=1`, `27=1`, `15=1`, `24=192`,
   `25=7`. Равное значение подавляет device call; изменённое кэшируется даже
   при ошибке `SetRenderState`.
5. Восемь texture identities `E480..E49C` получают `0xFFFFFFFF`, а color-source
   cache words `E4E8/E4EC` —11/10. Возвращается AL1.

COM readback остаётся внешним входом. Ненулевой HRESULT заменяет запись кэша
на invalid даже тогда, когда API уже записал output. Ни возвращённые значения,
ни пять принудительных записей не являются доказательством применённых GPU
состояний или полной таблицей игровых defaults. Missing provider означает
incomplete до изменений. Providers не должны менять или повторно вызывать
тот же selector/cache carrier. Reset получает реализацию primary slot `+44`
явно; для базового DX body она может вызвать этот восстановленный метод.

Эти операции дополняют [buffer protocol](../../reference/classes/sp-dx-buffers.md).

## Startup и освобождение прежнего устройства

[spDXRenderer_Startup.cpp](../../../Sparkplug/Code/SparkplugDX/spDXRenderer_Startup.cpp)
содержит body `4BD380`, общий helper `4548E0`, platform slot `+70` (`4BD700`)
и создание persistent index owner `4BD2D0`. Их
[carriers](../../../Sparkplug/Code/SparkplugDX/spDXRenderer_Startup.h) задают
известное состояние явно; они не заменяют native constructor или layout.

`4548E0` сначала вызывает platform slot `+70`, затем копирует32 configuration
bytes. Первый числовой input делится на3 с округлением вниз и передаётся
в общий `spIndexBuffer` как число primitives типа2 с format0. Второй input
задаёт размер raw `spVertexBuffer`. Эти helpers уже представлены общими
[index](../../reference/classes/sp-index-buffer.md) и
[vertex](../../reference/classes/sp-vertex-buffer.md) исходниками.
Оригинальный helper игнорирует оба AL и возвращает1; дополнительный отказ
переносимого allocation guard означает incomplete.

`4BD700` освобождает persistent index handle и его owned reference. Затем
обходит временные записи index buffers, вызывает release, уничтожает записи
и освобождает vector storage. Когда COM `Release` оставляет ненулевой refcount,
index wrapper сохраняет handle, и его destructor делает ещё один `Release`.
После index owners освобождается Device9, затем Direct3D9; возвращённые refcounts
не проверяются, slots обнуляются. Active byte secondary `+24` становится0.
Сам slot не задаёт определённого AL, поэтому переносимый bool сообщает только
о завершении операции.

Последовательность `4BD380`:

1. Выполнить общий helper и вызвать внешний `Direct3DCreate9` с SDK version32.
   NULL interface завершает startup с AL0. Adapter0 запрашивается для diagnostics.
2. Обнулить14 presentation words, вызвать `4BD050`, игнорируя его AL, затем
   `4BCCF0`. Применяются описанные выше выбор устройства и прямой output slot.
3. Обнулить offscreen/cache words. Создать новый `spDXIndexBuffer` owner,
   заменить прежнее owned отношение и вызвать его реальный Create wrapper с
   usage `0x208`, format `0x65`, pool0. Первый common input здесь передаётся
   как API byte size; это тот же numeric global, а не новая формула размера.
4. Установить active byte в1 и вызвать настоящий cache restore `4BCF20`.
5. Освободить прежнюю scratch array, выделить128 records по64 bytes и записать
   в каждый identity4×4. Record constructor `4A7B40` ничего не меняет; reset
   `434660` задаёт все16 words. Это отдельная scratch array, не утверждение
   о начальных world/view/projection matrices.
6. Получить или создать общий `spPCShaderManager` и выполнить его реальный
   [startup](../materials/shader-manager-key.md). Полный путь связывает directory
   scan с конкретным `LoadStandaloneFileForAnalysis`; внешними остаются file,
   CRT, XML, SDK assembler и COM interfaces. Неосвобождённые SDK interfaces
   записываются в аналитический retention carrier без выдуманного native owner.

Известные отказы создают error object и возвращают AL0. Они не отменяют уже
созданные renderer resources: например, отказ shader template происходит
после active=1 и выделения scratch array. Очистка выполняется явной последующей
операцией. Portable error presentation использует существующий nonmodal
`spPCErrorManager`; это техническая замена native modal UI.

Renderer state и shader/error globals имеют разное владение. Platform release
не уничтожает эти общие managers и не освобождает common CPU payloads или
scratch array. Shared host references сохраняют известные отношения безопасно;
native uint16 refcount overflow и reentry не моделируются. Повторный startup
с уже использованным shader manager отклоняется до изменения renderer:
исходный overwrite старого template без release не подменяется скрытым cleanup.

Наличие startup body и совпадение выбранных операций не устанавливают весь
renderer constructor/destructor, все размеры и resource families, работу
реального Direct3D/GPU либо визуальную готовность игры.
