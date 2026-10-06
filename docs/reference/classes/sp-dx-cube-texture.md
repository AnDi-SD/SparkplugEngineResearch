# spDXCubeTexture

`spDXCubeTexture` (`0x5C542AD9`) наследует [`spCubeTexture`](sp-cube-texture.md). [Реализация](../../../Sparkplug/Code/SparkplugDX/spDXCubeTexture.h) восстанавливает идентичность, клонирование и состояние присоединённой текстуры через явно обозначенное CPU storage. Живой графический backend не входит в этот класс.

PC конструктор `0x004B9150` устанавливает primary table `0x006F1940`, secondary table `0x006F192C` и обнуляет cube surface `+0x3C`, palette `+0x40`, byte count `+0x48`. Runtime format `+0x44` остаётся незаписанным до инициализации. Device `+0x38` удерживается через COM AddRef и освобождается через Release. Native размер — `0x4C` байт. CPU модель хранит semantic state без подражания ABI и опасному владению native palette.

Clone `0x004B9460` создаёт новый объект и вызывает наследованное копирование Named prefix. Имя копируется; cube surfaces, palette, runtime format и initialization state не копируются.

Runtime attachment `0x004B9630` записывает width, height, raw D3DFORMAT и byte count, устанавливает initialized, сбрасывает byte `+0x31` и удерживает поверхность. Поля режима `+0x18`, byte `+0x1C` и flags `+0x20` эта перегрузка не меняет. Затем attachment вызывает `0x004B91D0`, который перегенерирует каждый следующий mip через D3DXLoadSurfaceFromSurface.

CPU storage поддерживает шесть square faces с полной цепочкой степеней двойки и восемь подтверждённых raw D3DFORMAT values. Word cube payload передаётся непосредственно в `CreateCubeTexture`; enum ordinary `spDXTexture` для него не применяется. Для нескольких уровней требуется явно предоставленный `MipRegeneratorForAnalysis`: он отвечает за backend math, сохраняет размеры и base face bytes, а его context должен жить дольше текстуры. Без такого provider многослойная инициализация завершается отказом. Единственный уровень не требует фильтрации.

Оригинальный loop не проверяет HRESULT D3DX и возвращает `true` после обхода шести faces. Отказ portable provider и проверка его descriptors — явная политика нашего host, а не восстановленная native ветвь ошибки.

Чтение и запись выполняет [`spDXCubeTextureSerializer`](sp-dx-cube-texture-serializer.md). Byte count сохраняется до backend regeneration. Не восстановлены оригинальная D3DX filtering math, live COM/GPU lifetime, renderer palette registry и загрузка шести CPU buffers через аппаратную upload-ветвь.
