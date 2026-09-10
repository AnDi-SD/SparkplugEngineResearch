# Проекции: геометрия, FX и разные интерфейсы PC/PS2

10 сентября 2026. [Контракт11 имён](../../research/projection-family-contracts-2026-09-10.json):
восемь PC и девять PS2 записей. Две прежние PC оценки ProjectionManager48 и
PCProjectionManager52 сохраняются. Pristine hashes закреплены в контракте.

## Геометрические классы

spProjection — базовый Renderable для проекций; BoxProjection и PyramidProjection
задают два вида геометрии, ShadowProjection специализирует PyramidProjection.
Регистрация PS2 содержит ещё TextureProjection. Наличие имени в RTTI не означает
наличия factory или восстановленного полного runtime.

Box/Pyramid PC factory/getter/Clone/два удаления прошли, размеры184/208 байт.
PS2 factory независимо выделяют176/200 байт и вызывают ctor001BE1B0/001C0550;
оба вызывают Projection ctor001BE5D0, тот — Renderable001A9AC0. Base Projection
имеет зарегистрированную нулевую factory и null Clone на обеих платформах.

PC Shadow ctor005A3D10 вызывает Pyramid00432F30, PS2 ctor001C23F0 —001C0550.
У Shadow обе factory нулевые и оба Clone возвращают null. Первый scout window
`pc-Shadow-ctor-window` фактически включает getter/destructor: настоящий ctor
затем захвачен отдельно в `shadow-construction`. По соседству функций
назначение не переносится.

У Box/Pyramid Copy slot совпадает с Renderable: PC00423C70 /PS2001A9690.
Два новых полных PC опыта изменяют собственные геометрические слова перед Clone:
Box9C/A0/A4/A8/AC/B0, PyramidA0/A4/A8/AC/B0/B4/B8. В новой копии они остаются
**factory defaults**, как и FX94=0. Исходный объект сохраняется без изменений.
PS2 независимые таблицы подтверждают inherited Copy, но полный populated PS2
Clone этим не объявляется исполненным.

## ProjectionFX: объект начинается с другого интерфейса

spProjectionFX имеет основной интерфейс эффекта и CrossPlatform/BaseObject **+4**
на обеих платформах. PC base ctor0041A3A0 вызывает CrossPlatform на this+4;
PS2 ctor001BE830 делает то же через00105D60. Собственные source18 и ready byte1C
начально нулевые. Базовый secondary Clone возвращает null.

У конкретного PCProjectionFX primary table006F2880 содержит два метода эффекта,
secondary006F2864 — методы объекта. PS2ProjectionFX соответственно00491D70 и
00491D80 с двумя ABI header words; семь object slots дополнены тремя прямыми
реализациями getter/Clone/destructor. Это разные классы платформ, не одно RTTI имя.

Первый PCFX pilot ошибочно считал primary table таблицей BaseObject и попытался
исполнить строковые данные как RTTI getter. После проверки обеих native layouts
обвязка использует object pointer+4. Оригинальный Clone возвращает новый
**secondary pointer**, а allocator/free относятся к полному объекту. Исправленный
run2 прошёл factory/getter/Clone/два удаления за0,410 с, размер168 байт. PS2
factory выделяет80 байт; отдельные adjustors вычитают4 перед собственными методами.
Код игры не патчился, нарушение правил разработки не требовалось.

## Создание эффекта, повторный Setup и начальные параметры

[Probe](../../research/probe_projection_fx_contracts.py),1,634 с: два полных PC
опыта владения, четыре PS2 Setup prefix случая, шесть пар FX Init.

Setup PC004243D0 /PS2001BE3F0 создаёт FX при нулевом member94/8C, записывает
FX18=projection и вызывает primary Init, затем возвращает1. Повторный PC Setup
сохраняет тот же FX и не выделяет дополнительных объектов. PS2 missing-FX путь
останавливается перед настоящей factory0020B100; existing-FX prefix выполняет
оригинальный Init0020ADC0 для отсутствующего material dependency.

Оригинальные PC destructor удаляют оба Projection и созданный FX. Начальная
проверка «освобождены все выделения» дважды остановилась: остаётся один56-байтовый
объект с table006DC3B8, уже присутствовавший в прежних cold lifetimes. Его нельзя
объявлять утечкой без проверки глобального владения. Run3 требует удаления FX и
обоих Projection и отдельно учитывает тот же прежний context allocation.

Init PC004C4FF0 /PS20020ADC0 при ready1C!=0 сразу возвращает1, не трогая source.
При ready0 и нулевом dependency source20 ставит ready1 и возвращает1. В случаях
с dependency и0/1/3 layer entries подтверждены конкретные записи:

| Поле состояния слоя | PC | PS2 |
|---|---:|---:|
| 1C /20 | 2 /2 | 2 /2 |
| 30 /2C | B /A | 3 /80 |
| Флаг dependency | byte6D=1 | byte75=1 |

Dependency layers pointer находится PC+4C /PS2+54. Guarded literal records здесь
объявлены явно; это не реконструкция полной material factory. PC останавливается
на следующем настоящем allocator helper00460E50, до оставшихся действий Init.
PS2 целиком выполняет перенос этих битов и ready1 без платформенных вызовов;
LWC1/SWC1 в этом участке переносят биты, арифметики FPU нет. Значения двух
платформ не заменяются друг другом и не объявляются доказательством одинакового
GPU результата.

## Менеджеры и прежний неизвестный ID

[22 коротких PS2 опыта](../../research/probe_projection_ps2_leaves.py),0,424 с:
восемь настоящих RTTI getter routes, четыре null Clone, три secondary adjustor
routes и семь byte getter/setter/Init случаев. Manager getter001AD500 возвращает
byte14, setter001BE8E0 записывает low8 аргумента, Init001BEBE0 ставит byte14=1
и возвращает1. Эти slots одинаковы у base и PS2ProjectionManager. PS2 concrete
factory выделяет68 байт с alignment16 и вызывает base001BECB0; её собственные
remove/render phases отличаются от PC и здесь не исполнялись. Прежний полный PC
список/phase evidence — [отдельное досье](native-pc-scene-special-managers.md).

PS2 RTTI **spTextureProjection имеет ID58DA4026**. Это позволяет опознать такой
же literal в ранее изученном PC scene dispatch. В PC такого зарегистрированного
класса нет: PC factory или класс под это имя не придумываются. У PS2 TextureProjection
factory нулевая; используемый точный поиск constant getter не нашёл метода,
поэтому ему не присваивается чужая таблица. Пока учитывается только регистрация.

## Учёт и оставшиеся границы

Шесть новых PC оценок: Projection40, Box35, Pyramid35, ProjectionFX20, PCFX40,
Shadow15. Девять PS2: Projection30, Box20, Pyramid20, ProjectionFX20, PS2FX35,
ProjectionManager25, PS2ProjectionManager20, Shadow15, TextureProjection5.
Это15 новых assessments; новые fully closed классы не заявляются.

Открыты полная геометрия/проекционные матрицы, GPU phases, остальной PC populated
Init, глобальное владение context56, PS2 полные lifetimes и подтверждение FPU
арифметики. Основной исследовательский runner теперь учитывает доказанный +4
только для этого проверенного PCFX класса. Подготовка семейства и поиск точных
RTTI getter вынесены в общие ограниченные инструменты; результаты поиска остаются
кандидатами до проверки конструкторов и интерфейсов.
