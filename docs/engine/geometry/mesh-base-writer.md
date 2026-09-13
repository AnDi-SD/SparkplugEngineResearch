# PC общий MeshData writer

Policy0/2: BeginObject `472710`, WriteBegin(field0,sizecode7) `472D30`,
полный buffer helper `42B030`, WriteEnd `472E20`, terminator `472B00`.
Policy1 не вызывает buffer helper и сохраняет один нулевой byte. Никаких
PC-native полей у общего writer нет. Native writer policy1:1772 instructions;
policy0/2 около9800. Все CPU buffers/mesh/serializer/temporary list owners
освобождены; arena меньше1KiB на обычном triangle.
