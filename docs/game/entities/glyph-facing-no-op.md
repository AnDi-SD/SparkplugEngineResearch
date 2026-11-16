# Facing Glyph: no-op setter и FLD getter (PC)

`wxGlyph` связывает descriptor `Facing (rad)` собственного publisher с двумя различными короткими callbacks. Getter `0x53A480` выполняет `FLD DWORD PTR [ECX+0x15C]` и `RET`. Setter `0x5B7A00` состоит только из `RET 4`: он не читает аргумент или данные receiver и ничего не записывает. Название descriptor не даёт оснований добавлять изменение направления или запись F32.

Captured-value API принимает предоставленный префикс352байта с primary `0x701CE4`. `ReadFacingForAnalysis` расширяет finite normal binary32 или signed zero в точные10байт x87 с сохранением знака нуля через общий `FromFloat` provider. Subnormal, NaN и Infinity для getter-профиля пока не допускаются. Это ограничение нашего анализа, а не доказанный отказ оригинального getter.

`ApplyFacingNoOpForAnalysis` проверяет только собственные profile/primary и возвращает исходный префикс без изменений. Любые32бита аргумента игнорируются; даже недопущенное для getter поле здесь не становится причиной отказа. `ComposeFacingForAnalysis` моделирует GET/no-op/GET: две одинаковые10байтовые величины и неизменённый352байтовый префикс. Выход записывается лишь после допуска всей операции, prefix input/output могут совпадать. Значение `ResultForAnalysis` относится к нашему value API, а не native return signature.

Историческая allocation352байта совпадает с минимальным выбранным префиксом, но не устанавливает текущий sizeof, layout или физическое наследование. Factory/RTTI, typed clone, primary и регистрация атрибутируют descriptors; текущий FF25 constructor не раскрыт. Name descriptor, construction/lifecycle, native C++ типы и единицы, production dispatch, PS2, gameplay и целый класс остаются открытыми.
