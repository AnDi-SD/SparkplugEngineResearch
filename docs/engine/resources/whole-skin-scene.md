# PC: whole Skin scene и точность Node transform

Граф включает восемь Node, RenderNode, Skin, DXMaterial, Fog и DXMesh.
Skin с weightCount0 содержит16 bindings: `[4,10,11,12,13,4,4,4,4,4,4,4,4,4,4,4]`.
Все16 inverse-bind matrices сравниваются побитно. Материал, туман и common
MeshData загружаются тем же whole loader, без подмены результатов reader.
