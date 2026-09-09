# Инспекция Fog через общий serializer

Блок22 цикла9 сентября до19:00. Удалён ещё один C# payload decoder из
`SmoFogDecoder`; Inspector и Corpus через прежний API получают значения
настоящего `spFog`, прочитанные существующим `spFogSerializer`.

`spv_fog_payload_read` принимает ровно20 bytes. Для самостоятельного поля
общий `spDataBlockSerializer` создаёт временную секцию, затем общий Fog reader
читает её. ABI возвращает type/color/start/end/density без ограничения enum,
нормализации или преобразования IEEE bits. Это явная инспекция одного поля,
не synthetic resource graph и не доказательство полной загрузки окружающего
файла. Второй разбор формата в C# отсутствует; enum labels остаются UI data.

Три bounded original-PC capture (`values`, `raw-bits`, `logo-field`) совпали
с ABI и managed состояниями. Включены typeFFFFFFFF, negative zero, NaN payload
7FC12345 и infinity; все20 bytes сохранены. Настоящее поле Fog взято из
неизменённого `Menus/logo_screen.smo`; original factory/reader/writer/destructor
выполнены, все allocations freed. Пределы microprobes не повышались.

ABI:3 matches/7 guards. Managed:15 checks, включая существующий inspector
на игровом поле. Native FullLoader213 passed; пять consumer builds и финальный
FormatTests rebuild прошли без ошибок и предупреждений. Native игровая логика
не менялась; проверены новый bridge и его потребители. Полный корпус не запускался.

Артефакты: `local-data/results/tools-core-cycle-20260909-1900/fog-inspection/`.
Seal: `research/tools-core-fog-inspection-block-2026-09-09.json`.
Исходное доказательство codec: [native-pc-fog-serialization.md](native-pc-fog-serialization.md).
Новый GPU fog pass и поддержка всех оставшихся spatial inspectors не заявлены.
