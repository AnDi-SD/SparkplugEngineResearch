# Общие классы движка: дополнительный проход

## Создание и общий контекст

Первый PC batch: **21 попытка, три процесса, 8,927 с**.
В окончательном наборе **17 полных lifetimes**, 92 завершенные class operations
с учетом partial cases. Для AnimationManager context отдельно исполнены
создание и удаление manager; эти вспомогательные операции не входят в 92.
PS2: 19 собственных factory/table identities и 39 оригинальных getters.
Остальные классы имеют только metadata/getter либо static table candidate,
а не доказанную созданием объектную таблицу.

spLightController (112 байт) и spMovieTextureController (32) после создания
реального AnimationManager проходят Clone и оба удаления. Registry controller
перед удалением manager пуст; global обнуляется. spMovieLayer (36 PC /20 PS2)
при тех же условиях проходит factory, но его Clone имеет отдельную dependency.
Ни один игровой constructor или registry callback не заменен успехом.

spCinematicManager (188 обе платформы) использует встроенный timer. Применен
уже существующий профиль с историческим именем `network-clock`: он исполняет
original Timer Start/Stop и подает literal 1200 только через timeGetTime.
Сетевые inputs этим профилем не подключаются. Полный PC lifetime проходит.
spTemplate (96 PC /80 PS2) проходит после подключения существующих char_traits
inputs; оригинальный std::string/container код продолжает исполняться.

Другие полные PC lifetimes: AudioSound248, CameraManager40, Cinematic36,
GUIManager80, GameCore544, ParticleSystemManager36, Quad72, Quad3D48,
StreamError40, SubtitleTrack48, SystemSettings276, TransformConstEval104,
TriangleStripper68. Это не утверждение о работоспособности всех их подсистем.

## TransformConstEval

Constructor задает нулевой position vector +10/+14/+18, axis (0,0,1) по +1C,
angle +28 =0, embedded spFunctionEval по +2C и bytes +64/+65 =0.
PC allocation и PS2 factory size равны 104. Embedded PC table 006EA9AC,
getter 00478610 указывает на spFunctionEval; его существующая реализация
вычисления не заменялась.

Evaluate использует три независимые условия:

Полностью отключенная PS2 ветка исполняет все три flag0 и сохраняет входные
данные. Для активной translation PS2 останавливается перед первым multiply;
для rotation — перед helper, для scale — перед dispatch. Оригинальная rotation
ветка PS2 содержит COP1 accumulator instructions; их поведение не выдается
за результат R4000 emulator. Все области входов/выходов защищены полным
сравнением байтов, PC объекты удалены после восстановления введенных flags.

## Остановленные случаи и границы доказательства

| Класс | Состояние |
| --- | --- |
| spAudioListener | factory/RTTI/Clone проходят; delete 0044FAA0 требует отсутствующий receiver +70. Эта зависимость уже была найдена ранее; первый учет PC15 помечен reviewed-existing. |
| spCameraViewLayer | factory/RTTI проходят; Clone останавливается 00412C01 на null referenced-object lookup. |
| spMovieLayer | после AnimationManager factory/RTTI проходят; та же отдельная Clone lookup dependency 00412C01. |
| spLightEntity | factory требует MSVCR71.vsprintf, затем OutputDebugStringA; полный lifetime пока не выполнен. |

Для LightEntity существующий bounded formatter расширен стандартным va_list указателем и `%u`. Оригинальная строка: `INI REQUEST: [u32] %s (default: %u)\n`. Сохраняются предел 255 output bytes и только явно поддержанные форматы. После форматирования новый run дошел до OutputDebugStringA; этот import не направляется в ОС хоста. Все промежуточные версии formatter/runner сохранены. Эта обвязка не считается логикой игры.

В PC не найден выбранный exact constant getter pattern для spColorEval,
spEvaluator и spTransformEval. Их нулевые factories и registrations записаны
как **metadata-only5, reviewed-existing**; таблицы и реализации не придуманы.
PS2 getters этих классов найдены и исполнены независимо. У PS2 ParticleFX,
BallisticPFX и VideoStream есть secondary adjustor aliases: unique direct-getter
reference не принимается автоматически за начало vtable.

Оценки PC: full lifetime20; TransformConstEval40; AudioListener15;
CameraViewLayer/MovieLayer15; LightEntity10; остальные getter-only10,
три metadata-only5. PS2: constructed identities15, TransformConstEval30,
остальные getter-only10. Новых полностью закрытых классов нет. Открыты
populated runtime, аппаратные/ресурсные зависимости, rendering/audio,
реальная работа TriangleStripper и остальные evaluator алгоритмы.
