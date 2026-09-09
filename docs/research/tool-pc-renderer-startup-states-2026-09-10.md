# PC renderer: подтверждённые startup states и граница backend

Оригинал PC подтверждает пять принудительных desired states. Это отдельный
результат для общего native backend descriptor; готовый OpenGL multipass и
успешная полная инициализация renderer этим блоком не объявляются.

## Адресная цепочка и результат

Primary-vtable slot `+0x44` у DX/PC (`006EFA84`, `006F295C`) содержит
`004BCF20`. Startup `004BD380` вызывает slot в `004BD58C`, после сообщения
`Renderer initialized`; reset `004BD810` — в `004BDA98`. Original source anchor:
`Z:\Sparkplug\Code\SparkplugDX\spDXRenderer_Init.cpp`.

Body `004BCF20..004BD04D` сначала вызывает общий invalidator `00454940`, затем
рассматривает render IDs 0..255 и texture IDs 0..63 на восьми stages. После
original selector `004BCEC0` успешные внешние `GetRenderState` COMvE8 и
`GetTextureStageState` COMv108 копируются в кэши; отклонённый selector или failed
readback записывает FFFFFFFF. Это внешние входы, не таблица игровых defaults.

Хвост вызывает общий cached writer `004B0A90` в следующем порядке:

| Call | Device state | Desired value |
|---|---|---:|
|004BCFCC|143 NORMALIZENORMALS|1|
|004BCFD7|27 ALPHABLENDENABLE|1|
|004BCFE2|15 ALPHATESTENABLE|1|
|004BCFF0|24 ALPHAREF|192|
|004BCFFB|25 ALPHAFUNC|7 GREATEREQUAL|

Две свежие пробы только original slice `004BCFB9..004BD04D` подтверждают порядок,
обновление кэша при HRESULT80004005 (**143 инструкции**) и отсутствие COM calls
при равном кэше (**103 инструкции**). ESI, четыре saved-register stack words и
256 device-cache words — явно объявленные входы среза. Original prologue,
selector и readback не выполнялись; harness проверил штатный финальный стек.
Остальные render-cache words, включая171, и все512 stage-cache words неизменны.
Игровые функции не заменялись; успешное применение состояний настоящим GPU
не заявляется: lower writer игнорирует HRESULT и кэширует desired value.

Отдельная проба полного `004BCF20` остановилась на существующем лимите100k/2s
в protected selector route `004BCEC0→00431760`, IP `00431794`, до первого
readback/write. Guest отброшен, не возобновлялся; selector не подменялся.
Во всех пробах неизменны30s child, один worker и micro caps. Static verifier:
9 точных byte anchors и5 decoded push/call mappings.

## Граница первоначальной пробы

BLENDOP171 и начальные texture argument states2/3/5/6 не получают forced writes
в этом body или известных material/texture maps. Они остаются unknown; SDK
defaults не подставлены как доказанные игровые значения. Material application
может заменить24/25: для BloomX material89 это alphaRef0 и func7. Уже доказанные
blend6 factors SRC_ALPHA/ONE и непустые passes `[0,6,6]` сами по себе не дают
полный backend state для готового draw.

Узкий read-only audit нашёл реальное происхождение device: startup вызывает
`004BCCF0` в `004BD499`; helper делает `CreateDevice` COMv40 в `004BCE3C`,
out-pointer `this+C9E8`. Factory `0060FBB0→IAT006D94EC` импортирует
`d3d9.dll!Direct3DCreate9` с SDK argument32. Далее перед state-init вызывается
dynamic-buffer helper `004BD2D0→004B2140`; сам buffer consumer уже исследован.
Эта связь подтверждена original bytes/import, но числовые начальные171/stage
args не наблюдались. В восстановленном `spDXRenderer_Init.cpp` пока только
RTTI; полноценного CreateDevice/startup source consumer нет.

Следующая конкретная граница: наблюдать успешный actual device readback171 и
stage args2/3/5/6 сразу после `004BCE3C`/перед `004BD58C`, с доказанным отсутствием
промежуточной записи этих states; для common cache отдельно нужен original
selector `004BCEC0`. Повтор capped fullbody с прежними входами не нужен. До
получения доказательства descriptor может отдать пять known desired values и
явную unknown mask, без C# копии алгоритма и без объявления BloomX multipass готовым.

Evidence: [машиночитаемый отчёт](../../research/tools-core-renderer-startup-boundary-2026-09-10.json).
Локальные listing, probes и captures:
`local-data/results/tools-core-cycle-20260910-0730/material-preview/render-state-boundary/`.
Production, UI, общий cycle report и builds этим блоком не менялись.

## Последующее уточнение

[Actual device probe](tool-pc-device-state-reference-2026-09-10.md) получил
render171=1 и stage0..7 args2/3/5/6=`[2,1,2,1]` сразу после original CreateDevice
на RTX3070. Прежняя unknown-граница сузилась: числовые значения наблюдались,
но их сохранность до `004BD58C` проверяется отдельно. Также установлено,
что холодный selector содержит конечный цикл более100k инструкций: прежняя
остановка не доказывает недостающую инициализацию среды. Первоначальные
неуспешные captures и их пределы сохранены без изменений.
