# PC `spActor`: playback, события и граница менеджера

## Что восстановлено

Tick использует `actor +0x1C` (apply) и `+0x24` (advance while disabled).
Оба false — выход даже без event flush. Иначе frame delta умножается на actor
`+0x20`, затем на playback `+0x30`; обрабатываются только running entries с
положительным binding-use count. Duration берётся из `spAnimation +0x14`.

Reverse применяется к sample time до endpoint clamp. Clamp вне диапазона
для mode 0/3 не учитывает reverse; при progress ровно 0/1 clamp не производится.
Elapsed растёт даже в mode 3. Transition duration очищается при `elapsed > duration`,
не при equality. Controller blend factor — `elapsed / transitionDuration`.

## Constructor и registry

Оригинальный factory `0x005A3620` выделяет exact `0x54`; constructor `0x005A3500`
вызывает protected `spController::0x00423010 -> 0x004C4210`.
Controller имеет physical `spSubController` base, enabled byte `+0x10`, next/prev
`+0x14/+0x18`. Он регистрируется через `0x00453450` в manager global `0x0075F880`:
head/tail находятся по `+0x24/+0x28`. Destructor снимает регистрацию через `0x00453480`.
