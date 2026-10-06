# spPS2CubeTexture

`spPS2CubeTexture` (`0x18062B6E`) — конкретная PS2 оболочка [`spCubeTexture`](sp-cube-texture.md). [Исходники](../../../Sparkplug/Code/SparkplugPS2/spPS2CubeTexture.h) сохраняют фабрику, Named Clone и **оригинальные аппаратные заглушки** игры. Эти успешные no-op hooks найдены в executable; они не добавлены для обхода отсутствующей реализации.

Фабрика `0x001F37B0` выделяет ровно `0x38` bytes, вызывает cube constructor `0x001738E0`, затем устанавливает primary table `0x00491770` и secondary table `0x00491794`. Собственных полей нет. Clone `0x001F3660` создаёт такую же оболочку и вызывает наследованное Named copy `0x00105DC0`: сохраняется имя, а initialized state остаётся новым.

| Offset от secondary vptr | Adjustor | Body | Результат |
| --- | --- | --- | --- |
| `+0x14` | `0x001F3820` | `0x001F35B0` | `false`, без записей |
| `+0x10` | `0x001F3830` | `0x001F35C0` | `false`, без записей |
| `+0x0C` | `0x001F3840` | `0x001F35D0` | no-op return |
| `+0x08` | `0x001F3850` | `0x001F35E0` | `true`, без записей |

Исходные сигнатуры hooks неизвестны; аналитические facades описывают их неизменный результат, не приписывая неизвестным аргументам типы. Secondary vptr указывает на `0x00491794`; первые два слова таблицы — ABI prefix. Adjustors вычитают `0x14` из receiver. Primary slot `0x48` также использует `0x001F35E0`.

Общая cube initialization `0x00173770` получает размеры первой face, записывает аргумент в `+0x18`, нулевой byte `+0x1C`, flags в `+0x20`, размеры в `+0x28/+0x2C` и initialized. При запросе нормализации используется общий texture normalizer и обновляется byte `+0x30`. Затем primary slot `0x48` возвращает `true`, хотя аппаратная текстура не создана. State-only API сохраняет это отличие между initialized state и существованием backend storage.

Это описание не распространяет успешный no-op на обычные PS2 textures, VRAM cache или cube render targets: у них самостоятельные классы и методы.
