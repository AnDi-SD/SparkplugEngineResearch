#!/usr/bin/env python3
"""Check the thin matrix-writing ABI against sealed original PC writer output."""
from pathlib import Path
import ctypes as C
import hashlib
import json
import sys

ROOT = Path(__file__).resolve().parents[1]
def sha(path):
    return hashlib.sha256(Path(path).read_bytes()).hexdigest().upper()

def main(original_path, output_path):
    original = json.loads(Path(original_path).read_text(encoding='utf-8'))
    assert original['status'] == 'passed'
    assert sha(ROOT/'local-data/pc-pristine/WinxClub.exe') == original['pc_exe_sha256']
    for item in original['dependencies']:
        assert sha(ROOT/item['path']) == item['sha256'], item['path']
    library = ROOT/'artifacts/native/viewer/Release/SparkplugViewerNative.dll'
    dll = C.CDLL(str(library))
    dll.spv_last_error.restype = C.c_char_p
    dll.spv_static_write_matrix_fields.argtypes = [C.c_void_p, C.c_void_p]
    dll.spv_static_write_matrix_fields.restype = C.c_void_p
    dll.spv_serialized_bytes_size.argtypes = [C.c_void_p, C.POINTER(C.c_uint32)]
    dll.spv_serialized_bytes_copy.argtypes = [C.c_void_p, C.c_void_p, C.c_uint32]
    dll.spv_serialized_bytes_destroy.argtypes = [C.c_void_p]
    rows = []
    for case in original['cases']:
        data = C.create_string_buffer(bytes.fromhex(case['matrices_hex']))
        handle = dll.spv_static_write_matrix_fields(data, C.byref(data, 64))
        assert handle, dll.spv_last_error().decode()
        try:
            size = C.c_uint32()
            assert dll.spv_serialized_bytes_size(handle, C.byref(size))
            assert size.value < 1024
            result = C.create_string_buffer(size.value)
            assert dll.spv_serialized_bytes_copy(handle, result, size.value)
            assert result.raw.hex() == case['output_hex'], case['name']
            rows.append({'name': case['name'], 'bytesExact': size.value})
        finally:
            dll.spv_serialized_bytes_destroy(handle)
    output = Path(output_path).resolve()
    output.relative_to(ROOT/'local-data/results')
    assert not output.exists(), 'Preserve existing evidence'
    output.write_text(json.dumps({'status': 'passed', 'scope': 'five archived original scalar writers vs new authoring ABI',
        'originalPath': str(Path(original_path)), 'originalSha256': sha(original_path),
        'dependencyChecks': len(original['dependencies']) + 1, 'nativeSha256': sha(library), 'cases': rows}, indent=2)+'\n', encoding='utf-8')
    print(f'Static authoring: {len(rows)} exact original/ABI outputs')

if __name__ == '__main__':
    main(*sys.argv[1:])
