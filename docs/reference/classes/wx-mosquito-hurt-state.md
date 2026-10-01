# `wxMosquitoHurtState`

`wxMosquitoHurtState` (Class ID `66597187`) — потомок [wxCharacterState](wx-character-state.md) с selector `10`, исходным размером `3C` и без дополнительных полей. [Общая реализация](../../../Winx/Code/wxMosquitoHurtState.cpp) не задаёт игровой ABI.

| Операция | PC | PS2 |
| --- | ---: | ---: |
| Фабрика | `403C80` | `3F10A0` |
| Vtable | `6F9AC0` | `499590` |
| Вход, PC slot `1C` | `521200` | `2F5270` |
| Выход, PC slot `20` | `5211C0` | `2F5240` |
| Permission, PC slot `34` | `523770` | `2F53D0` |

Вход задаёт `(key & FF808001) | 8001`, выполняет lookup и запускает найденный handle с `mode=false`, `interrupt=true`, не освобождая старый. Затем сохраняет handle, вызывает виртуальное обновление и передаёт consumer setter скорости `2.0f`; возвращает `true`. Обновление унаследовано от базы и пусто. Выход передаёт setter `1.0f`, возвращает `true` и оставляет pending handle прежним.

Permission всегда вызывает consuming completion query, включая null handle. Slot `38` возвращает `false`. Остальные hooks и Reset унаследованы; Copy пуст, clone имеет начальные привязки и flags. Деструктор не останавливает анимацию.

Для скорости нужен обязательный [wxCharacterSpeedStateHost](../../../Winx/Analysis/Host/wxCharacterSpeedStateHost.h). PS2 setter `2A6C20` записывает float в `consumer->field134->field20`; PC `4FB6B0` содержит runtime-переходник, и его внутреннее поведение остаётся внешней зависимостью. [Проверка](../../../Winx/Tests/wxAdditionalCharacterStateTests.cpp) охватывает вызовы входа/выхода и consuming query; полный consumer и игровая сцена этим интерфейсом не подменяются.
