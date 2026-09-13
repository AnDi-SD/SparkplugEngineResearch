# Платформенные классы движка: PC и PS2, 10 сентября 2026

## Объекты и платформенные различия

Полные PC lifetimes: AsmShaderParser16, AudioBank40, AudioBankEntry260,
AudioManager344, AudioVoice56, BallisticPFX80, ShaderEffect36,
ShadowVolume244, ShadowVolumeMesh68, TextureManager44, PCBloomFX140,
PCFXFileLoader1344, PCPixelShader84, PCThread36, PCVideoStream36,
WindowsError40, WindowsFont4524 (число — байты оригинальной аллокации).
Имена без PC-префикса в этой строке относятся к `spDX*`.

Примеры независимо установленных размеров PS2: AudioBank44,
AudioBankEntry148, AudioManager308, AudioVoice60, BallisticPFX36,
CubeTexture56, DynamicMeshData256, GamePad204, Keyboard80, Mouse224,
Light256, Texture320, TextureManager40, VRAMCacheManager212,
VideoStream36. Все записи и остальные размеры — в контракте.
Нулевые registered factories: PC DXBloomFX/DXInputDevice/DXPixelShader,
PS2 InputDevice. Нулевой factory сам по себе не доказывает абстрактность.

У PC BallisticPFX, PCThread и PCVideoStream интерфейс BaseObject расположен
на +4. Первый прогон ошибочно использовал primary table: Thread/Video
вернули посторонние значения, Ballistic вышел за таблицу. Исходники игры
не менялись: исправлен адрес интерфейса в исследовательском runner.

| PC класс | Primary table | BaseObject +4 | Clone | deleting adjustor |
| --- | ---: | ---: | ---: | ---: |
| spDXBallisticPFX | 006EF8E8 | 006EF8CC | 004AC860 | 004AC6A0 |
| spPCThread | 007291FC | 007291E0 | 006BE630 | 006BE5B0 |
| spPCVideoStream | 006F2AC4 | 006F2AA8 | 004C73B0 | 004C7330 |
