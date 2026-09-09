# PC: настоящий device как эталон начальных состояний

После оригинальных mapper/CreateDevice срезов на RTX3070 получены все33
недостающих значения: render state171 =1 и texture-stage states2/3/5/6
= `[2,1,2,1]` для каждого stage0..7. Все HRESULT равны0. Это наблюдение
на данном устройстве с объявленными исследовательскими windowed options;
полный startup игры, shipped options, pixels и OpenGL equivalence не проверены.

## Что действительно выполнено

Pristine PC SHA256 `3F022480BF55045DA4BF692E4BC8862ED38FC024E8A964A558FBDFDF646DFC4F`.
Внешние Win32/D3D9 вызовы обслуживает отдельный x86 worker через настоящую
`SysWOW64/d3d9.dll`; игровые функции не заменяются успешными заглушками.
Factory `0060FBB0` передаёт SDK32. Original mapper `004BD050..004BD284`
преобразует явно заданные64×64 windowed options; остановка предшествует logger.
Свежий guest использует полученные options, затем original `004BCCF0`
и caps helper `004BCC80` доходят до actual COM CreateDevice в `004BCE3C`.
Последний срез останавливается в `004BCE3F`, перед logger/cookie tail.
Соответственно выполнены2,63 и120 инструкций.

Mapped и post-create D3DPRESENT_PARAMETERS совпадают:
`[64,64,22,1,0,0,1,0,1,1,80,0,0,0]`.
После успешного CreateDevice worker читает171 и32 stage arguments с того же
device. Эти значения не подставлены из SDK. Числовая пара `[2,1]` означает
TEXTURE/CURRENT; render171=1 означает ADD.

Время полного успешного процесса2,834с. Верхняя граница суммы отдельных peak
working sets180039680B (171,7МиБ); одновременный peak отдельно не измерялся.
Сохранены100k/2s micro и30s process caps; один worker, скрытое окно, kill-on-close
Job Object. Device/D3D Release вернули0, окно уничтожено, worker завершился0.
Original EXE не изменён; полный игровой процесс не запускался.

## Причина первой неудачной пробы

Тот же helper под restricted token песочницы получил `8876086A NOTAVAILABLE`
в GetDeviceCaps, до CreateDevice. Read-only диагностика уже там видела RTX3070,
nvldumd.dll и обычную Console session1: это не RDP/Basic Render Driver.
Идентичные probe/worker bytes при разрешённом запуске вне песочницы прошли.
Вторая диагностика показала тот же адаптер/session, medium integrity и
неповышенный token; изменился `token_restricted` true→false. Администраторские
права для успешной пробы не потребовались. Первая неудача сохранена отдельно.

Это основание учитывать контекст процесса при следующих адресных GPU-пробах,
а не повторять медленный реверс после инфраструктурного NOTAVAILABLE.

## Уточнение предыдущей остановки selector

Свежий prefix `004BCEC0(171)` дошёл до `00431780` за1638 инструкций без function
seams. Original count104, IDs включают171; холодный цикл перебирает256
кандидатов. Статический подсчёт наблюдённого body даёт108339 инструкций только
для цикла и epilogue: прежний100k cap был заведомо недостаточен. Отсутствие
device/environment этим не доказано; старый остановленный guest не продолжался.

Original BL сбрасывается для каждого кандидата, но cache write находится
после всего цикла. Свежий объявленный last-iteration slice с EDX255/ECX104/
ESI171 завершился за535 инструкций: cache[171]=0, lazy flag=0, AL=0.
Это ограниченная проверка последней итерации, не полный selector capture.
Необычное поведение не исправлялось и successful cache не подставлялся.

## Оставшаяся граница

Полученные значения относятся к моменту сразу после CreateDevice. Последующий
callgraph audit закрыл отсутствие промежуточных state-setting операций до
startup `004BD58C` для явно объявленного fresh/default контекста: old owner
`+CA28=NULL`, pristine logger callback и allocator hooks. Единственная device
операция — CreateIndexBuffer, без binding. Original constructor/logger/allocator
срезы выполнили3229/1612/35/19 инструкций, без function seams и повышения caps.
Повторного GPU readback в `004BD58C` нет; произвольные custom callbacks,
reused-owner cleanup и shipped startup этим не доказаны.

Полная runtime state-cache инициализация и multipass draw здесь не закрыты.
Следующий нужный источник — default material `+C9C0`: прежние pass probes
передавали явно собранный fixture, а обычный Material constructor создаёт ноль
passes. Нельзя приписать такому fixture original fallback states stages1..7.
Также остаются original shader constants/light inputs и современный shader.

[Manifest с hashes](../../research/tools-core-device-state-reference-2026-09-10.json)
ссылается на локальные `material-preview/device-reference*` и
`material-preview/selector-dependency` artifacts текущего цикла. Их нет в Git.
