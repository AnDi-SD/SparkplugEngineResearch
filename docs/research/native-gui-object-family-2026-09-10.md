# Семейство GUIObject: конструкция, состояния и сообщения

10 сентября 2026. Шесть общих классов PC/PS2 прежде не имели отдельной оценки.
PC EXE SHA256 `3F022480BF55045DA4BF692E4BC8862ED38FC024E8A964A558FBDFDF646DFC4F`;
PS2 ELF SHA256 `198313352DBF4FA26FF8C5D509F6783FC32F9B504A627E416323C5FFBBFFE8FE`.
[Машинный контракт](../../research/gui-object-contracts-2026-09-10.json)
содержит file offsets, исходные байты, независимые getter/registration проверки
и SHA локальных свидетельств. Capture — ограниченное линейное окно, не CFG.

| Класс / область | Размер PC / PS2 | Vtable PC / PS2 | PS2 construction entry |
| --- | ---: | --- | --- |
| spGUIObject: базовый GUI Entity | 60 / 56 | 6DCC70 / 48E560 | 1659F0 inline factory; отдельный ctor165860 |
| spWidget: выбор обычного/выключенного/выделенного узла | 84 / 80 | 6DEA6C / 48D810 | 16C1F0 |
| spButton: дополнительное нажатое состояние | 96 / 92 | 6DEAC0 / 48D7D0 | 169210 |
| spEditBox: объект редактирования текста; активный ввод ещё открыт | 128 / 124 | 6DEB14 / 48D750 | 16ABF0 |
| spTextWidget: текстовый потомок Widget; собственное изменение текста открыто | 84 / 80 | 6DBD20 / 48D790 | 134690 inline factory |
| wxButton: игровой потомок Button; игровые действия ещё открыты | 424 / 420 | 6FE988 / 496900 | 3742E0 |

Размеры получены отдельно из PC allocation и PS2 factory. Разница четыре байта
относится только к этим шести классам. Factory TextWidget вызывает Widget ctor,
затем пишет собственную vtable; этот вызов не является отдельным TextWidget ctor.
GUIObject factory также встраивает собственную конструкцию после Entity ctor.
Вызов106C60 с адресом конца GUIObject возвращает переданный указатель без записи:
это не доказательство выхода записи за allocation. Отдельный ctor165860 вызывает
Entity14E390(1), устанавливает vtable, byte28=1 и обнуляет три слова member2C.

## Проверенные границы PC

Все шесть factory/getter/default clone/delete-clone/delete-original прошли:
30 class operations. Pilot GUIObject0,826 с; пакет остальных пяти2,610 с,
четыре успешных. wxButton run1 остановился на CRT strncpy; run2 с уже проверенной
bounded-strings fixture завершился за0,565 с. Никакой выдуманный GUI context
не добавлялся. После удаления class objects остаются tracked allocations
общего окружения: два у пяти engine классов, пять у wxButton; полного shutdown
окружения и отсутствия утечек этот опыт не доказывает.

Own defaults Widget: visible=1, highlight=0, current/три state pointers/index=0.
Button дополнительно создаёт двухбайтовый буфер с первым нулём; pressed=0,
pressed Node=null. wxButton имеет четыре строки `empty`, отдельный byte1A4/1A0=1
и регистрацию в engine/game managers. EditBox обнуляет собственные pointer/word
поля и три байта; запись в padding не подразумевается.

## Переключение состояний: 18 пар

[Probe](../../research/probe_gui_widget_state_selection.py): PC полные оригинальные
4358C0/435CB0 с четырьмя настоящими Node factory421E20 и оригинальным Node enable.
PS2 отдельные fresh prefixes16BBCC/168D4C, остановка у реального consumer1A5B00
либо до LQ restore. После скрытия старого Node второй fresh prefix начинается
16BC10/168D90 с явно объявленным ранее проверенным контрактом Node Enable;
полное PS2 исполнение метода не заявляется. Время18 пар6,264 с.

| Поле | PC | PS2 |
| --- | --- | --- |
| enabled / visible / highlight | 28 / 3C / 3D | 28 / 38 / 39 |
| current / normal / disabled / highlighted | 40 / 44 / 48 / 4C | 3C / 40 / 44 / 48 |
| state index | 50 | 4C |
| Button pressed / pressed Node | 58 / 5C | 54 / 58 |

Если текущий Node существует и его mask200 уже снят, метод сразу возвращается.
Иначе старый Node получает Enable(0,1). При enabled=0 выбирается index1, иначе
highlight ненулевой выбирает2, обычный —0. Отсутствующий state Node заменяется
normal Node, но index не меняется. Новый ненулевой Node получает
Enable(visible,1), включая ненормализованный байт2. Старый и новый Node могут
совпадать: оба вызова выполняются.

Button при ненулевых pressed и pressed Node выбирает этот Node раньше проверки
enabled и сохраняет прежний index. Если pressed Node отсутствует, используется
обычный алгоритм Widget. Проверены disabled/highlight precedence, null fallback,
all-null, скрытый текущий Node, повторный выбор и pressed при enabled=0.
Guard охватывает весь receiver и все PC Node records; разрешено только изменение
current/index и исходного Node mask200. Названия полей — исследовательские.
[Ранее установленный Node contract](native-class-sp-node.md) используется повторно.

## Сообщения и setters: 27 пар

[Probe](../../research/probe_gui_widget_dispatch.py),3,842 с. SetEnabled4358A0 /
16BCC0 и SetVisible4358B0 /16BCA0 записывают младший байт аргумента, затем вызывают
виртуальный slot11. В Widget это state selection, в Button — его override.
Даже равный прежнему byteA5 аргумент вызывает refresh. Проверены0,1,255,
12345680 иA5; остановки находятся у настоящих consumer entries, результат
callback не подставляется.

GUIObject Notify4293B0 /1655D0 при code18 пишет this по указателю message+18;
при code1C передаёт текущий enabled byte в собственный метод4290B0 /165240,
в остальных проверенных случаях возвращается без изменения объектов.
Проверены соседние коды, enabled0/255, guard receiver/message/output.
Полное выполнение GUIObject enable со сценой этим опытом не закрывается.

## Ограничение эмулятора и исправление стенда

Первый selection run остановился на original MOVZ16BC28 с interrupt20:
модель R4000 этой инструкции не исполняет. Исходный run и source сохранены.
В [PS2 wrapper](../../research/ps2_scalar_prefix.py) добавлен явно выбираемый
integer-movz режим:5KC и allowlist обычных целочисленных инструкций этих окон.
Игровые байты не изменены. SQ/LQ, MMI, COP2, floating point, HI/LO и инструкции
вне списка запрещены; это не полноценный R5900. Исходный R4000 режим сохранён.

Девять целевых tests прошли за0,361 с: original MOVZ в branch delay slot при
нуле, ненулевом low32 и ненулевом только upper32; original LW sign extension,
старый отказ R4000, exact stops, выход из окна, single-use и EE guards.
Семантика low64 MOVZ/LW дополнительно сверена с
[исходником интерпретатора PCSX2](https://github.com/PCSX2/pcsx2/blob/master/pcsx2/R5900OpcodeImpl.cpp);
это вспомогательная проверка CPU, не замена evidence оригинального ELF.

## Оценка и открытая работа

Первичная оценка GUIObject30/25, Widget40/35, Button35/30,
EditBox/TextWidget/wxButton20/15 соответственно PC/PS2. Конструирование и
отдельные активные методы не означают полной изученности класса.
Открыты GUIObject Node/controller binding и восстановление controller flags,
полные Notify потомков, текстовые операции, игровой Button action flow,
заполненное клонирование, реальная сцена и полное PS2 исполнение.
Разработка UI/tools и упаковка релиза не выполнялись.
