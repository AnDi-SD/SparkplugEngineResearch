# Tick: пауза и переходы четырёх триггеров

## Порядок переходов

| Класс | Подтверждённое собственное управление |
| --- | --- |
| ChangeCharacterPlacement | Сначала cooldown. При его true проверяет v19. Для active=0 и принятой близости вызывает v21; для active!=0 и отвергнутой близости очищает ровно byte active. |
| OpeningGate | При active!=0 вызывает v20; без active сразу возвращаетtrue. Cooldown и v19 здесь не вызываются. |
| PivotingDoor / PushButton | Сначала собственный enabled byte. При enabled=0 возвращаетtrue. Для active=0 проверяет cooldown, затем v19 и приtrue вызывает v21. Для active!=0 сразу проверяет v19, без cooldown; приfalse вызывает v22. После завершения этих ветвей отдельный queued byte вызывает v20. |

Общий active находится в PC126/PS2132. Enabled/queued: Pivot PC168/169,
PS2180/181; Push PC148/149, PS2160/161. Эти рабочие названия описывают
использование, исходные имена полей пока не установлены.

## Границы описания

PS218 префиксов:11 собственных выходов,3 границы cooldown3AC940,
2 входа v20 OpeningGate и2 входа v19 двери/кнопки. Настоящий getter паузы
исполняется полностью. Ни cooldown result, ни geometry result не подставляются;
полный PS2 Tick с SQ/LQ и дочерними вызовами остаётся открытым.

Восемь updates +5: ChangeCharacterPlacement/PivotingDoor/PushButton PC25→30,
PS220→25; OpeningGate PC20→25/PS215→20. Полностью закрытых классов и новых
C++ реализаций нет. Исходная семантика дальнейших callbacks, нормальная
конструкция зависимостей и cache misses этим блоком не закрыты.
