# Native vertex layout → общий Remix converter

13 сентября 2026. Продолжение [native geometry](winx-remix-native-mesh-source-2026-09-13.md)
и этапов 1–2 [прямой сцены](winx-remix-direct-scene-plan-2026-09-12.md).

**Поддержанные native meshes теперь передают также описание вершин через
общий восстановленный emitter.** Фактическая D3D declaration остаётся
проверкой соответствия на каждом draw; материал и часть render states ещё
получаются через D3D. Самостоятельный scene lifecycle не появился.

## Одна реализация и границы

Тело `spPCVertexDeclaration::BuildElementsForAnalysis` без изменения логики
выделено в `Sparkplug/Code/SparkplugPC/spPCVertexDeclarationElements.h`.
Прежний класс вызывает эту функцию, native adapter использует её напрямую.
Это восстановленный PC4C9A00 → protected13D6F00 emitter, подтверждённый
[исследованием declaration](native-class-sp-vertex-declaration.md).
Ошибки реконструкции здесь не исправлялись. Сохранены native offsets,
приоритеты flags, terminator и особенность advance16 у компонента 0x400.

Собственная обвязка кеширует до 256 форматов по native `vertexComponentFlags`.
Ошибка allocation возвращает отказ. Пакет geometry заимствует готовые
элементы; после сравнения с actual COM declaration общий converter читает
именно этот массив. Нет второго расчёта offsets и отдельного API draw.

Проверка на каждом draw необходима: оригинальный declaration cache допускает
reinitialize объекта без смены ключа flags. При различии байтов/числа элементов
этот draw использует прежний D3D источник. Это общий контракт, без исключений
по ресурсам. Остальные ограничения native geometry предыдущего блока сохраняются.

## Проверка

- До rebuild сохранены старый EXE и `--capture`. Target-only rebuild общего
  `SparkplugVertexDeclarationTests`: **338/338**. Все **106 записей / 8656 bytes**
  совпали; SHA256 обоих captures
  `A5B148A891A8F47036FC869E2D50CB716641BCD6C24975C1C5C126271C3198C4`.
  Новых native эмуляций не было: переиспользовано прежнее полное evidence.
- Production D3D9 adapter + recording Remix API: **402 проверки**, включая
  171 native-source check. Проверены фактический layout, усечённый count,
  несовпадение flags/offset, fallback и возвращение на native source.
  Это собственный ABI fixture, отдельно от оригинальной игры.
- Surface resource lifetime: **57/57**. x86 и x64 DLL собраны; x64 native
  перехват по-прежнему отключён. Review замечания по `bad_alloc` и очистке
  состояния fixture исправлены до финальной x86 проверки.

### Оконная Алфея, native-layout-v2

Run `play-rtx-20260913-003207-848`, PID11880 завершён. Установленный adapter:
`A0FF8AE6983A13D5DC74D309F1C2FC549BA1326314E31CF326E2708F33AC706E`.
Stock client/server/renderer не менялись.

Записаны 380 полных native кадров по **871 instance**, всего **330980**.
518 сравненных поколений buffers, 0 upload/layout mismatch, 0 failure/limit;
log 1068260 bytes, лимит не достигнут. Все 2613 sampled submits используют
`native_flags_shared_emitter`, flags `0x940`; кеш содержит три формата.
Это не процент всех игровых мешей: в вызовы также входят другие passes/UI.

Сравнение в том же диалоге Алфеи: native off на frame684, on на frame721.
View, projection и позиция игрока в сохранённых A/B states совпадают точно.
Окружение/интерфейс видны в обоих режимах; кадры различаются анимацией.
`layout-native.png` снят ещё на splash. `layout-motion.png` имеет название
по попытке ввода: персонаж всё ещё в диалоге, свободное движение этим run
**не подтверждено**. Для него остаётся следующая игровая проверка камеры.

`user.conf`, `winx.ini`, saves и bridge config совпали с резервными копиями.
Bridge config восстановил штатный скрытый watcher. Повторный ручной restore
отказался от уже восстановленного файла; последующее сравнение подтвердило,
что он равен `bridge.conf.before`, изменений поверх него не делалось.

Машинное evidence с хешами:
[research/winx-remix-native-layout-2026-09-13.json](../../research/winx-remix-native-layout-2026-09-13.json).
Функциональная готовность изображения остаётся около 45%: изменился источник
данных, временная unlit → emission политика и незавершённые группы сохраняются.

Отдельно готов [пакет camera bridge](../../research/rtx-remix/direct-camera-bridge/README.md):
866 serializer checks и 20 isolated Camera → Draw → Present кадров. Следующий
шаг — подключение source camera на игровом render thread и проверка порядка
до первого main draw, без изменения renderer и без дополнительного Present.
