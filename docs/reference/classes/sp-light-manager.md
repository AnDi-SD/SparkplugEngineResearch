# spLightManager

Описание отдельных известных частей класса. Наличие карточки не означает полного восстановления всех методов.

Общие исходники: [spLightManager](../../../Sparkplug/Code/Sparkplug/spLightManager.h).

## Scene attachment и world refresh

`46AC40(renderNode)` сбрасывает count114 и ambient110, затем `46AA40`
проходит lights в порядке manager-list и заново отбирает cache. Unused slots
не очищаются. Actual SceneManager45A7D0 → RenderNode world4250F0 → light
selection исполнен; это не ручная подстановка готового cache результата.

Node helper `420DE0(bool)` рекурсивно переключает flag100, независимо от
Enabled200. Он не отсоединяет узлы, не меняет scene links/lists и не обновляет
кэши сам. Portable Node получил именно этот узкий helper. После flag-only
изменения следующий явный native rebuild меняет выбор света.

## Helper cache: восемь обычных и один ambient

Исходное имя helper-типа неизвестно. ABI `spLightCacheObservedLayout` и host
`CacheForAnalysis` — аналитические обозначения, не восстановленные символы.
Размер28: восемь pointers0..1C, ambient20, count24.

- `490B20` очищает все28 bytes.
- `490B50 → 441210` хранит первый ambient отдельно. Обычные pointers идут в
  порядке добавления, duplicates игнорируются, capacity8. Protected constant
  `13B3688` подтверждён после первой безопасной original call, до ninth add.
- `490BA0`: ordinary middle removal заменяет pointer последним и оставляет
  unused tail stale; removal последнего очищает tail. Ambient удаляется только
  при точном совпадении. Cache не владеет объектами и не меняет refs.
- `46AC40` resetSelection очищает count/ambient, но сохраняет raw slots.

Add/remove выбирают ветку по **текущему type** самого light. Восстановление
не обещает автоматической миграции между ambient/ordinary при изменении type.
Повреждённый native count>8 не исполнялся: original guard сравнивает equality,
а не обеспечивает общее восстановление повреждённого контейнера.

## Light/LightData: закрытые прежние неизвестные

Original41A330 выделяет **exactF0**. LightData primary6DE990 имеет15 slots,
поддержка6DE98C; padding и opaqueDC остаются untouched. Native copy
`428EB0 → 505CB0` независимо подтвердил прежний PS2 результат: type/color/
attenuation/opaque/range/angles/shadow/enabled копируются, **intensityD8 нет**.
Existing destination intensity9.5 сохраняется при source3.25; fresh actual
LightData clone41ACA0 остаётся1.0. Runtime membership links не копируются.

Новый `spLightManager` воспроизводит borrowed stable list, blank clone,
eligibility, selection reset, add/remove и refresh одного light. Host list
vector заменяет intrusive pointers, guards duplicate/4096 явно host-only.
Нет implicit Scene owner, automatic attach/world/partition/debug wiring.
Полный portable Node world virtual dispatch остаётся открытым.

Открыты: original header/API/support type, opaque lightDC, setters/dirty8,
конкретные partition payloads, SceneInit и complete portable scene integration,
backend light upload и full native root-clone transaction.
