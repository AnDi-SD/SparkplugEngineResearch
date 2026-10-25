"""Public shader-stage inventory; source coverage is not runtime qualification."""
from __future__ import annotations
import json
import posixpath
import re
from pathlib import Path, PurePosixPath

REGISTRY = 'docs/reference/shaders.json'


def load(root: Path) -> dict:
    return json.loads((root / REGISTRY).read_text(encoding='utf-8'))


def validate(root: Path, public: set[str]) -> list[str]:
    if REGISTRY not in public:
        return []  # Small documentation fixtures need not contain this collection.
    errors = []
    data = load(root)
    version = data.get('schemaVersion')
    if version not in (1, 2):
        return ['shader registry: unsupported schema']
    rows = data.get('programs', [])
    ids, origins, entries, sources = set(), set(), set(), set()

    def target(path: str, label: str) -> bool:
        parts = PurePosixPath(path).parts
        valid = bool(path) and not PurePosixPath(path).is_absolute() and not (
            {'..', '.private', 'local-data', '.codex-tmp'} & set(parts)) and ':' not in path and '\\' not in path
        if not valid or path not in public or not (root / path).is_file():
            errors.append(f'shader registry: {label} missing/nonpublic target {path}')
            return False
        return True

    for row in rows:
        label = row.get('id', '')
        if not re.fullmatch(r'pc\.[a-z0-9.-]+', label) or label in ids:
            errors.append(f'shader registry: invalid/duplicate ID {label}')
        ids.add(label)
        stage = row.get('stage')
        if row.get('platform') != 'pc' or stage not in ('vertex', 'pixel'):
            errors.append(f'shader registry: invalid platform/stage for {label}')
        original = row.get('original', {})
        identity = original.get('file'), original.get('recordIndex')
        if identity in origins:
            errors.append(f'shader registry: duplicate original stage {identity}')
        origins.add(identity)
        file = original.get('file', '')
        rfx = file.endswith('.rfx')
        index = original.get('recordIndex')
        if (PurePosixPath(file).name != file or not file or
                (rfx and (type(index) is not int or index < 0)) or
                (not rfx and (index is not None or not file.endswith('.vsh' if stage == 'vertex' else '.psh')))):
            errors.append(f'shader registry: invalid original identity for {label}')
        prefix = 'vs' if stage == 'vertex' else 'ps'
        if original.get('profile') != prefix + '_1_1':
            errors.append(f'shader registry: original stage/profile mismatch for {label}')
        reconstruction = row.get('reconstruction', {})
        path, entry = reconstruction.get('source', ''), reconstruction.get('entryPoint', '')
        pair = path, entry
        if pair in entries:
            errors.append(f'shader registry: duplicate reconstruction entry {pair}')
        entries.add(pair)
        sources.add(path)
        if (reconstruction.get('profile') != prefix + '_3_0' or
                reconstruction.get('status') != 'semantic-reconstruction'):
            errors.append(f'shader registry: reconstruction profile/status mismatch for {label}')
        if target(path, label):
            text = re.sub(r'/\*.*?\*/|//[^\n]*', '', (root / path).read_text(encoding='utf-8'), flags=re.S)
            if not re.fullmatch(r'[A-Za-z_][A-Za-z0-9_]*', entry) or not re.search(r'\b' + re.escape(entry) + r'\s*\(', text):
                errors.append(f'shader registry: missing source entry for {label}')
        target(row.get('documentation', ''), label)
        for name, value in reconstruction.get('defines', {}).items():
            if not re.fullmatch(r'SPARKPLUG_[A-Z0-9_]+', name) or type(value) is not int:
                errors.append(f'shader registry: invalid compile define for {label}')
    expected_files = data.get('pcExternalScope' if version == 2 else 'scope', {}).get('originalFileCount')
    if len({row.get('original', {}).get('file') for row in rows}) != expected_files:
        errors.append('shader registry: original file count disagrees with stages')
    for path in public:
        if path.startswith('Sparkplug/Shaders/PC/') and path.endswith('.hlsl') and path not in sources:
            errors.append(f'shader registry: unlisted reconstruction {path}')
    executable = data.get('executable', {})
    for path in executable.get('specializationSources', []) + executable.get('documentation', []):
        target(path, 'executable')
    if version == 2:
        if set(data.get('scope', {}).get('platforms', [])) != {'pc', 'ps2'}:
            errors.append('shader registry: version 2 requires explicit PC/PS2 scope')
        spans = []
        for item in data.get('binaryPrograms', []):
            label = item.get('id', '')
            if not re.fullmatch(r'ps2\.vu\.[0-9a-f]{8}', label) or label in ids:
                errors.append(f'shader registry: invalid/duplicate binary ID {label}')
            ids.add(label)
            if item.get('platform') != 'ps2' or item.get('kind') != 'vu-upload-image':
                errors.append(f'shader registry: invalid binary program kind for {label}')
            original = item.get('original', {})
            file = original.get('file', '')
            if not file or PurePosixPath(file).name != file:
                errors.append(f'shader registry: invalid binary container name for {label}')
            address = original.get('containerAddress', '')
            sizes = [original.get(name) for name in ('containerByteLength', 'payloadBytes', 'instructionPairs', 'uploadChunks')]
            if (not re.fullmatch(r'0x[0-9A-F]{8}', address) or
                    any(type(value) is not int or value <= 0 for value in sizes)):
                errors.append(f'shader registry: invalid binary range for {label}')
            else:
                container, payload, instructions, chunks = sizes
                if payload != instructions * 8 or container != 16 + payload + chunks * 8:
                    errors.append(f'shader registry: inconsistent upload sizes for {label}')
                spans.append((file, int(address, 16), int(address, 16) + container))
            if item.get('extractionStatus') != 'structure-validated' or item.get('semanticStatus') != 'unrecovered':
                errors.append(f'shader registry: unsupported binary recovery claim for {label}')
            if item.get('unit') is not None or item.get('runtimeQualified') is not False:
                errors.append(f'shader registry: unsupported hardware qualification for {label}')
            target(item.get('documentation', ''), label)
        for previous, following in zip(sorted(spans), sorted(spans)[1:]):
            if previous[0] == following[0] and previous[2] > following[1]:
                errors.append('shader registry: overlapping binary program containers')
        completeness = data.get('completeness', {})
        requirements = completeness.get('requirements', [])
        requirement_ids = [item.get('id') for item in requirements]
        expected = {'input-cohort', 'external-resources', 'pc-embedded-and-generated',
                    'captured-program-source-identity', 'ps2-upload-images',
                    'producer-dispatch-closure', 'material-constant-packet-contracts', 'behavior-and-scenes'}
        statuses = {'open', 'partial', 'bounded-complete', 'complete'}
        if set(requirement_ids) != expected or len(requirement_ids) != len(expected) or any(
                item.get('status') not in statuses for item in requirements):
            errors.append('shader registry: missing/invalid completeness requirements')
        if completeness.get('status') not in ('open', 'complete') or (
                completeness.get('status') == 'complete' and any(item.get('status') != 'complete' for item in requirements)):
            errors.append('shader registry: completeness claim exceeds its requirements')
        if data.get('scope', {}).get('completeness') != completeness.get('status'):
            errors.append('shader registry: scope/completeness status disagreement')
        target(completeness.get('documentation', ''), 'completeness')
        for platform, inventory in data.get('platformInventories', {}).items():
            if platform not in ('pc', 'ps2'):
                errors.append(f'shader registry: unknown inventory platform {platform}')
            target(inventory.get('documentation', ''), platform)
    return errors


def document(root: Path) -> str:
    data = load(root)
    programs = data['programs']
    files = {p['original']['file'] for p in programs}
    vertex = sum(p['stage'] == 'vertex' for p in programs)
    source_files = {p['reconstruction']['source'] for p in programs}
    lines = [
        '# Каталог шейдеров', '',
        f'Полный список исследованной внешней PC-папки `Shaders`: **{len(files)} файлов, {len(programs)} стадий** '
        f'({vertex} vertex и {len(programs) - vertex} pixel). Восстановленное HLSL-представление находится '
        f'в **{len(source_files)} исходных файлах**. Стадия, семейство и сгенерированный вариант учитываются отдельно.', '',
        '[Машиночитаемый реестр](shaders.json) · [Карточки](shaders/README.md) · '
        '[Исходники](../../Sparkplug/Shaders/README.md) · '
        '[Исходный shader-контракт](../engine/materials/shader-contract.md)', '',
        '## Программы', '',
        '| ID | Ресурс / запись | Стадия | Назначение | Карточка | Восстановленный код |',
        '| --- | --- | --- | --- | --- | --- |'
    ]
    for item in programs:
        original, source = item['original'], item['reconstruction']
        resource = original['file'] + (f":{original['recordIndex']}" if 'recordIndex' in original else '')
        doc = posixpath.relpath(item['documentation'], 'docs/reference')
        code = posixpath.relpath(source['source'], 'docs/reference')
        lines.append(f"| `{item['id']}` | `{resource}` | {item['stage']} | {item['purpose']} | "
                     f"[{item['name']}]({doc}) | [{source['entryPoint']}]({code}) |")
    binary = data.get('binaryPrograms', [])
    if binary:
        lines += ['', '## Программы внутри executable', '',
            'PS2-образы учитываются отдельно от PC vertex/pixel-стадий. '
            'Границы DMA/VIF-контейнера и payload установлены; семантическое '
            'восстановление инструкций и аппаратное исполнение остаются открытыми.', '',
            '| ID | ELF VA контейнера | VU payload, байт | Instruction pairs | MPG-блоки | Описание |',
            '| --- | --- | ---: | ---: | ---: | --- |']
        for item in binary:
            original = item['original']
            doc = posixpath.relpath(item['documentation'], 'docs/reference')
            lines.append(f"| `{item['id']}` | `{original['containerAddress']}` | {original['payloadBytes']} | "
                         f"{original['instructionPairs']} | {original['uploadChunks']} | [Контракт]({doc}) |")
        lines += ['', 'Единица VU для этих путей не квалифицирована. Образ, MPG-блок, '
                  'callback и entry point не считаются одной и той же единицей покрытия. '
                  'Полные извлечённые bytes/listings остаются локальными данными.', '',
                  '[Поиск в executable и условия полноты](../engine/materials/shader-discovery.md).']
    lines += ['', '## Что означает восстановление', '',
        'Каждая запись имеет читаемое семантическое HLSL-представление. Исходные assembly 1.1 выражены '
        'через именованные вычисления; текстовые RFX-шаблоны реконструированы с явными параметрами. '
        'Это не заявление о восстановлении оригинальных имён, побитовом совпадении инструкций, '
        'всех GPU-пограничных значений или готовности адаптера Remix.', '',
        'Исходные стадии имеют shader model 1.1. Профили 3.0 в реестре служат для проверки читаемой '
        'реконструкции; они не меняют версию оригинала и не означают установку этих программ в игру.', '',
        '## Генерация и код EXE', '',
        '`Fixed.rfx` порождает варианты по числу весов, ColorMode, упорядоченному свету, specular, '
        'числу UV-выходов и UV-маске. Это одна параметризованная стадия, а не отдельная запись на '
        'каждую комбинацию. Нулевой вес в штатном manager обходит создание такого VS.', '',
        'EXE содержит [выбор вариантов](../engine/materials/shader-generation.md), '
        '[сборку/компиляцию](../engine/materials/shader-compile.md) и '
        '[подготовку констант](../engine/materials/shader-constants.md). '
        'Они учитываются отдельно от GPU-программ. Наличие SDK assembler/compiler не доказывает '
        'наличия встроенного shader payload.', '',
        '## Границы полного списка', '',
        'Перечень полон для исследованной внешней PC-папки, включая обе стадии каждого RFX. '
        'Из PS2 ELF выделены указанные выше upload-образы. Исчерпывающая инвентаризация всех '
        'runtime-only источников PC/PS2, версий игры и дополнительных пакетов остаётся открытой. '
        'Создание программы при старте не доказывает её использование '
        'при draw; отдельный наблюдавшийся набор не доказывает покрытие всех уровней.', '',
        'Наличие HLSL, успешная компиляция, численное сравнение и проверка настоящей игры — '
        'разные уровни проверки. Реестр учитывает исходники и контракты; текущие игровые результаты '
        'и происхождение проверок хранятся отдельно.', '']
    return '\n'.join(lines)
