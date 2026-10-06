# spMasterTimer

Общие исходники: [spMasterTimer.h](../../../Sparkplug/Code/Sparkplug/spMasterTimer.h),
[spMasterTimer.cpp](../../../Sparkplug/Code/Sparkplug/spMasterTimer.cpp).
Класс `287E268B` имеет физическую и регистрационную базу
[spTimer](sp-timer.md), `149C778B`. Native размер PC и PS2 — `0x28`:
первые `0x24` байта принадлежат таймеру, `+24` содержит secondary vtable singleton
interface. Portable C++ layout не совпадает с native multiple inheritance.

Default construction сначала инициализирует остановленный `spTimer`, затем
публикует адрес полного объекта в singleton global. Поля таймера остаются
начальными. На PC primary vtable — `6E693C`, secondary — `6E6938`, global —
`75DB74`; на PS2 — `48CAE0`, `48CB04`, global — `49F848`.

Clone создаёт новый master timer с default payload и публикует его вместо прежнего
instance. Унаследованный Copy — успешный root no-op; исходные flags и время не
копируются. Start, Stop и Reset полностью наследуют операции `spTimer`.

Destructor безусловно очищает global перед cleanup таймера. Если после создания
второго объекта уничтожить первый, global станет null, хотя второй объект ещё
существует. Portable версия сохраняет эту особенность. PC secondary deletion
thunk `450580` вычитает `0x24` из receiver и переходит к primary destructor
`450690`; PS2 `115220` выполняет такое же смещение к `114FB0`.

`GetInstanceForAnalysis` предоставляет чтение literal global pointer. Собственный
host отвечает за владельцев объектов; lazy acquisition, глобальное завершение
игры и полная политика singleton interface не восстановлены. Реализация внешних
часов остаётся границей [spTimer](sp-timer.md).
