# PC: whole loading.smo с текстурой и общей геометрией

## Граф и общий путь

TextureData wire ID `78EA082B` разрешается через DXData serializer42B660;
header42DD10 создаёт runtime DXTexture `3F3651B6`. Embedded reader42C640
вызывается дважды, включая внутренний source wrapper. Затем actual4ABBA0,
4AB030 и четыре61039A/60FDB4 вызова создают полный набор пяти уровней16×16…1×1.
Все1364 texture bytes сопоставлены с source. Из остальных данных734 байта
приходятся на object state,170 на material layers,688 на mesh buffers/declarations.
