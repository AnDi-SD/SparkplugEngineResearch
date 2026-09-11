"""Pack existing original Init captures, retaining unknown allocator words as masks."""
from pathlib import Path
import json,struct,sys
ROOT=Path(__file__).resolve().parents[1]
OLD=ROOT/'local-data/results/tools-core-cycle-20260910-0730/particle-loop-init'
def pack(paths,destination):
    result=bytearray(b'PTI1'+struct.pack('<I',len(paths)))
    def block(raw):result.extend(struct.pack('<I',len(raw)));result.extend(raw)
    def u32(*values):result.extend(struct.pack('<'+'I'*len(values),*values))
    for path in paths:
        report=json.loads(path.read_text());before,after=report['pools']
        assert report['status']=='captured-loop-init-return'
        block(path.stem.encode());block(bytes.fromhex(before['parametersHex']))
        block(bytes.fromhex(report['renderNodeInput']['worldTransformHex']))
        for random in (before['random'],after['random']):
            u32(random['index']);block(bytes.fromhex(random['stateHex']))
        u32(after['count'],after['first'],after['boundary'],*[after[k] for k in ('active','free','deltaBits','clockBits','accumulatorBits')])
        block(bytes.fromhex(after['recordsHex']));block(b''.join(struct.pack('<3I',*link) for link in after['links']))
        # Original lifetime is initialized even in a free record. Other words
        # are valid only for the emitted suffix of physical slots during Init.
        assert after['active']+after['free']==after['count']
        block(bytes([0]*after['free']+[1]*after['active']))
    destination.write_bytes(result)
    print(f'{len(paths)} original cases, {len(result)} capture bytes -> {destination}')
if __name__=='__main__':
    paths=[OLD/'pc2-bg-init-run2.json',*[OLD/f'pc2-bg-count-{n}.json' for n in (127,128,129)]]
    if len(sys.argv)>1:
        folder=Path(sys.argv[1]);paths += [folder/(n+'.json') for n in ('zero-world','directed-local','parallel-basis','right-angle','exact256','nonloop')]
    pack(paths,ROOT/'Sparkplug/Tests/Fixtures/particle-init.dat')
