# Audio: cached параметры менеджера и состояние голоса

## Параметры менеджера

PC4C3E50 возвращает32,PS21ED820 —48. Это исходные значения конкретных
virtual queries,а не измеренная пропускная способность физических устройств.

## PS2 AudioVoice

PC4CCC70 при нулевом obj28 возвращаетAL0. Ненулевой device pointer ведёт
кCOM status query с последующим bit0 mask,но этот путь пока только виден
статически. Полный PC/PS2 audio hardware parity из этого не следует.

Pilot1:3 attempts /0,305911s,два successes иCOM argument-order oracle error.
Pilot2:3 /0,211998s;batch32 /2,214146s. Guard4KiB,неизменные PS2 code bytes,
SDK cache,полный frame у whole returns;5 stack-spill operations.
Повторной общей CPU регрессии нет:wrapper не менялся.
