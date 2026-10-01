# `wxMosquitoMovingState`

`wxMosquitoMovingState` (Class ID `6F324B81`) — потомок `wxCharacterState` с selector `0`. [Общая реализация](../../../Winx/Code/wxMosquitoMovingState.cpp) сохраняет собственное обновление PC/PS2. Исходный размер — `3C`, дополнительных полей нет. Переносимый объект не является игровым ABI.

| Операция | PC | PS2 |
| --- | ---: | ---: |
| Фабрика | `403CE0` | `3F0FA0` |
| Vtable | `6F9B08` | `499540` |
| Обновление, PC slot `30` | `5212D0` | `2F5480` |

Обновление сначала задаёт `(key & FFFFFFF1) | 1`. Затем читает float из control word `4`: control расположен по `owner+12C` на PC или `owner+138` на PS2. Порог — float32 `0.005f`, битовое значение `3BA3D70A`.

При `motion < threshold` применяется `key &= FFFFFF8F` и обнуляется control word `4`. В остальных случаях задаётся `(key & FFFFFFDF) | 50`, control не обнуляется. Затем применяется маска `F007FFFF` и выполняется lookup. Равенство порогу выбирает вторую ветвь. В PC x87-варианте туда же попадает NaN; семантика специальных значений EE FPU отдельно не квалифицирована.

При изменении handle выполняется playback с `mode=true`, `interrupt=true`, без освобождения старого handle и без сброса completion-записей. Handle сохраняется после playback. Одинаковый handle пропускает запуск, однако предшествующие изменения ключа/control всё равно выполняются.

Вход базы освобождает pending handle и вызывает виртуальное обновление. Прочие hooks, Copy и reset используют [wxCharacterState](wx-character-state.md). Slots `34/38` — общие `true/false` PC hooks и собственные эквивалентные PS2 leaves. Clone получает свой тип и начальные runtime-поля, деструктор не останавливает анимацию.

[wxMosquitoMovingStateHost](../../../Winx/Analysis/Host/wxMosquitoMovingStateHost.h) предоставляет чтение control и общие анимационные зависимости без успешных заглушек. [C++ проверка](../../../Winx/Tests/wxDroidMosquitoStateTests.cpp) охватывает float32-границу, порядок callbacks, одинаковый handle, отсутствие old release и clone. Полная игровая сцена остаётся внешней зависимостью.
