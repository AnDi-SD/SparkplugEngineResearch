# spLightController

Общие исходники: [spLightController](../../../Sparkplug/Code/Sparkplug/spLightController.h).
Восстановлены PC constructor/defaults, прямое обновление цвета, RTTI, clone и
собственный lifetime. Обновление использует общий восстановленный
[spColorFuncEval](../../../Sparkplug/Code/Sparkplug/spColorFuncEval.h) и его ограниченный finite sampler.
Это не проверка запуска игры, графического backend или всех IEEE входов.

## Идентичность и поля

ClassID `0x10262533`, регистрационная база `spController` (`0x4FAD24F1`).
Размер объекта на PC и PS2 — `0x70`; portable C++ хранение не повторяет native ABI.

| Смещение | Поле | PC default |
| ---: | --- | --- |
| `0x10` | enabled, inherited Controller | true |
| `0x14`, `0x18` | intrusive links, inherited Controller | null |
| `0x1C` | вложенный spColorFuncEval | собственный объект |
| `0x2C`, `0x30` | Color1, Color2 ARGB | `0xFF000000` |
| `0x34` | вложенный spFunctionEval | собственный объект |
| `0x44` | sampler time | `0.0` |
| `0x48`, `0x4C`, `0x50` | frequency, reciprocal, amplitude | `1.0` |
| `0x54`, `0x58`, `0x5C`, `0x60` | x offset, y offset, pitch, clamp limit | `0.0` |
| `0x64` | clamp enabled byte | false |
| `0x68` | raw function type | `0` |
| `0x6C` | borrowed Light | null |

Нативные указатели и padding описаны отдельно в
[PC ABI](../../../Sparkplug/Analysis/PC/spLightControllerAbi.h).
Класс не удерживает и не освобождает Light. Вызывающий код отвечает за его lifetime.

## Прямое обновление

PC `0x42F930` при null Light возвращает управление без вычисления sampler и
изменения состояния. При ненулевом Light вызывает вложенный ColorFuncEval с
исходным float delta. Тот отдельно масштабирует и усекaет два цвета по каждому
байту, складывает байты и возвращает packed ARGB.

Затем `0x424700` переводит байты в RGBA float: R, G, B, A умножаются на
исходную float константу с битами `0x3B808081`. Полученные четыре float записываются
в Light `+0xC4..0xD0`, а в flags `+0xB0` добавляется бит `8`.
Прямой метод не читает enabled; менеджер отдельно решает, вызывать ли контроллер.

Host `TryApplyForAnalysis` возвращает false при отказе общего finite sampler,
не меняя Light; virtual `ApplyForAnalysis` превращает этот отказ в исключение.
Оригинал такой проверки и bool результата не имеет. Точность nonconstant
sampler и host sin ограничена документированными finite vectors; соответствие
всех finite значений x87, NaN и иных режимов округления не заявлено.

## Clone и платформы

PC `0x41A9D0` создаёт новый контроллер, регистрирует пару в CloneManager и вызывает
унаследованный Controller copy: enabled переносится, вложенный evaluator и Light
остаются defaults. Intrusive links не копируются.

PS2 constructor имеет тот же extent и offsets, но default ARGB равен `0`.
PS2 update `0x11A170` делит байты на `255`, записывает RGBA в Light `+0xD0..0xDC`
и добавляет бит `8` в `+0xB4`. Эта portable реализация квалифицируется по PC;
PS2 fractional conversion и полноценный renderer остаются отдельной работой.
