"""Fetch pinned upstream inputs; generate our light adaptation outside tracked sources."""
from pathlib import Path
import argparse
import hashlib
import json
import subprocess
import urllib.request

ROOT = Path(__file__).resolve().parents[2]
HERE = Path(__file__).resolve().parent
REFERENCE = ROOT / 'local-data/rtx-remix/upstream/dxvk-remix'
OUTPUT = ROOT / 'local-data/rtx-remix/dependencies/include'


def sha(data):
    return hashlib.sha256(data).hexdigest()


def git(*args):
    return subprocess.check_output(['git', '-C', str(REFERENCE), *args], text=True).strip()


def write_verified(path, data):
    path.parent.mkdir(parents=True, exist_ok=True)
    if not path.exists() or path.read_bytes() != data:
        path.write_bytes(data)


def adapt_light(source):
    """Same CPU subset as before relocation; original function bodies come from NVIDIA."""
    notice = source[:source.index('*/') + 2]
    body = source[source.index('static float leastSquareIntensity('):
                  source.index('Vector3 LightUtils::calculateRadiance(')]
    replacements = {
        'float discriminant = b * b - 4.0 * a * c;':
            'float discriminant = static_cast<float>(b * b - 4.0 * a * c);',
        'return sqrt(endDistanceSq);': 'return static_cast<float>(sqrt(endDistanceSq));',
        'float LightUtils::calculateIntensity(const D3DLIGHT9& light, const float radius)':
            'static float calculateIntensity(const D3DLIGHT9& light, const float radius, float gain)',
        'LightManager::calculateLightIntensityUsingLeastSquares()': 'true',
        'LightManager::lightConversionIntensityFactor()': 'gain',
        'LightManager::lightConversionMaxIntensity()': 'FLT_MAX',
        'std::min(': '(std::min)(',
        'std::max(': '(std::max)(',
    }
    for before, after in replacements.items():
        if before not in body:
            raise RuntimeError(f'Upstream light adaptation anchor changed: {before}')
        body = body.replace(before, after)
    body = '\n'.join(line.rstrip() for line in body.split('\n'))
    prefix = '''

// Adapted from NVIDIA dxvk-remix b81a7b566b1eeb9edb4dc2b3c9d3972e0f253ad4.
// CPU-only subset: explicit gain, original default least-squares branch, no GPU
// dependencies. Formula is Remix policy, not reconstructed Winx/Sparkplug code.
#pragma once
#include <algorithm>
#include <cfloat>
#include <cmath>
#include <d3d9.h>
namespace remix_light_conversion {
constexpr float kLegacyLightEndValue=1.0f/255.0f;
constexpr float kNewLightEndValue=0.01f;
constexpr float kPi=3.14159265358979323846f;
'''
    return (notice + prefix + body + '}\n').encode('utf-8')


def prepare(offline=False):
    lock = json.loads((HERE / 'dependencies.json').read_text(encoding='utf-8'))
    remix = lock['remix']
    if not REFERENCE.exists():
        if offline:
            raise RuntimeError('Run tools/WinxRemix/prepare_dependencies.py once with network access')
        REFERENCE.parent.mkdir(parents=True, exist_ok=True)
        subprocess.run(['git', 'clone', '--no-checkout', remix['repository'], str(REFERENCE)], check=True)
        subprocess.run(['git', '-C', str(REFERENCE), '-c', 'core.autocrlf=false',
                        'checkout', '--detach', remix['revision']], check=True)
        subprocess.run(['git', '-C', str(REFERENCE), 'remote', 'set-url', '--push',
                        'origin', 'DISABLED_READ_ONLY_REFERENCE'], check=True)
    if git('remote', 'get-url', 'origin') != remix['repository'] or git('rev-parse', 'HEAD') != remix['revision']:
        raise RuntimeError('Reference must use the pinned official Remix repository and revision; existing checkout preserved')
    # Verify only consumed files, allowing separately initialized upstream submodules.
    inputs = {}
    for relative, expected in remix['inputsLfSha256'].items():
        data = (REFERENCE / relative).read_bytes().replace(b'\r\n', b'\n')
        if sha(data) != expected:
            raise RuntimeError(f'Pinned upstream input changed: {relative}')
        inputs[relative] = data
    light = adapt_light(inputs[remix['lightSource']].decode('utf-8'))
    if sha(light) != lock['generatedLightLfSha256']:
        raise RuntimeError('Generated light conversion differs from the previously verified adapter')
    xxhash = lock['xxhash']
    xxpath = OUTPUT / 'third-party/xxhash.h'
    if xxpath.exists():
        xxbytes = xxpath.read_bytes()
    else:
        if offline:
            raise RuntimeError('xxHash missing: run tools/WinxRemix/prepare_dependencies.py with network access')
        with urllib.request.urlopen(xxhash['url'], timeout=30) as response:
            xxbytes = response.read(2 * 1024 * 1024)
    if sha(xxbytes) != xxhash['sha256']:
        raise RuntimeError('xxHash checksum mismatch; dependency was not accepted')
    write_verified(xxpath, xxbytes)
    write_verified(OUTPUT / 'third-party/remix_light_conversion.h', light)
    metadata = {'schema': 1, 'remixRevision': remix['revision'],
                'generatorSha256': sha(Path(__file__).read_bytes()),
                'lockSha256': sha((HERE / 'dependencies.json').read_bytes()),
                'generatedLightLfSha256': sha(light), 'xxhashSha256': sha(xxbytes)}
    write_verified(OUTPUT / 'third-party/dependencies.json', (json.dumps(metadata, indent=2) + '\n').encode())
    return OUTPUT


if __name__ == '__main__':
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument('--offline', action='store_true', help='Verify and generate without downloading')
    args = parser.parse_args()
    print(prepare(args.offline))
