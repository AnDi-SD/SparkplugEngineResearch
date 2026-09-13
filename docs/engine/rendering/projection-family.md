# Проекции: геометрия, FX и разные интерфейсы PC/PS2

## Геометрические классы

spProjection — базовый Renderable для проекций; BoxProjection и PyramidProjection
задают два вида геометрии, ShadowProjection специализирует PyramidProjection.
Регистрация PS2 содержит ещё TextureProjection. Наличие имени в RTTI не означает
наличия factory или восстановленного полного runtime.

Box/Pyramid PC factory/getter/Clone/два удаления прошли, размеры184/208 байт.
PS2 factory независимо выделяют176/200 байт и вызывают ctor001BE1B0/001C0550;
оба вызывают Projection ctor001BE5D0, тот — Renderable001A9AC0. Base Projection
имеет зарегистрированную нулевую factory и null Clone на обеих платформах.

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

## Создание эффекта, повторный Setup и начальные параметры

Setup PC004243D0 /PS2001BE3F0 создаёт FX при нулевом member94/8C, записывает
FX18=projection и вызывает primary Init, затем возвращает1. Повторный PC Setup
сохраняет тот же FX и не выделяет дополнительных объектов. PS2 missing-FX путь
останавливается перед настоящей factory0020B100; existing-FX prefix выполняет
оригинальный Init0020ADC0 для отсутствующего material dependency.

Init PC004C4FF0 /PS20020ADC0 при ready1C!=0 сразу возвращает1, не трогая source.
При ready0 и нулевом dependency source20 ставит ready1 и возвращает1. В случаях
с dependency и0/1/3 layer entries подтверждены конкретные записи:

## Менеджеры и прежний неизвестный ID

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
