#!/usr/bin/env python3
"""Full original/source compressed missing-mip reader comparison."""
from pathlib import Path
import hashlib,json,subprocess,sys
from pc_instruction_emulator import ROOT,run_bounded
from probe_pc_texture_compressed_missing import main as native,specimen


def main(kind='dxt5',dimension='8',pattern='random',label='first'):
    data,_=specimen(kind,int(dimension),pattern);binary=ROOT/'.codex-tmp/Sparkplug-build-pc2100-utf8/SparkplugTextureSerializationTests.exe'
    result=subprocess.run([str(binary),'--missing-native'],input=data.hex()+'\n',text=True,capture_output=True,timeout=10)
    assert result.returncode==0,result.stderr;source=json.loads(result.stdout)
    observed=native(kind,dimension,pattern,'compare-'+label,return_capture=True);state=observed['state']
    expected={'state':[state[0],state[1]&255,state[2],state[3]&255,state[4],state[5]],'levels':observed['levels']}
    assert all(block['dither']==0 for block in observed['encodedBlocks'])
    differences=[{'level':i,'native':a,'source':b} for i,(a,b) in enumerate(zip(expected['levels'],source['levels'])) if a!=b]
    report={'kind':'native-source-compressed-missing-mips','status':'passed' if source==expected else 'mismatch','codec':kind,
            'dimension':int(dimension),'pattern':pattern,'payloadSha256':observed['payloadSha256'],
            'sourceExecutableSha256':hashlib.sha256(binary.read_bytes()).hexdigest().upper(),
            'exactMipBytes':sum(len(row['packedHex'])//2 for row in observed['levels']) if source==expected else None,
            'generatedMipBytes':sum(len(row['packedHex'])//2 for row in observed['levels'][1:]) if source==expected else None,
            'stateEqual':source['state']==expected['state'],'differences':differences,
            'original':{k:v for k,v in observed.items() if k not in ('events','encodedBlocks','payloadHex','levels')}}
    path=ROOT/f'local-data/results/cycle-20260908-0700/cp113-compressed-comparison-{kind}-{dimension}-{pattern}-{label}.json'
    path.write_text(json.dumps(report,indent=2)+'\n');print(json.dumps(report,sort_keys=True));return int(source!=expected)


if __name__=='__main__':
    raise SystemExit(main(*sys.argv[2:]) if sys.argv[1:2]==['--guest'] else run_bounded(Path(__file__),sys.argv[1:]))
