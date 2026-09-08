"""Compare strict C# wire palettes with the original PC loader's live objects."""
from pathlib import Path
import hashlib,json,struct,sys

def sha(path):return hashlib.sha256(Path(path).read_bytes()).hexdigest().upper()

def main(input_path,output_path,*native_paths):
    source=Path(input_path);data=json.loads(source.read_text(encoding='utf-8-sig'))
    assert 1<=len(native_paths)<=5 and len(data['cases'])==len(native_paths)
    natives={}
    for path in native_paths:
        native=json.loads(Path(path).read_text(encoding='utf-8-sig'))
        assert native['status']=='passed' and not native['diagnostics'] and not native['remainingAllocations']
        assert native['inputSha256'] not in natives
        natives[native['inputSha256']]=(native,path)
    rows=[];bones=0
    for case in data['cases']:
        reference=source.parent/case['reference'];assert sha(reference)==case['referenceSha256']
        expected=json.loads(reference.read_text(encoding='utf-8-sig'));assert sha(expected['smo'])==expected['smoSha256']
        native,path=natives[expected['smoSha256']]
        actual={int(k):v for k,v in native['scene']['objects'].items() if v['classID']==0x681f2043}
        assert set(actual)=={skin['id'] for skin in expected['nativeSkins']}
        for skin in expected['nativeSkins']:
            value=actual[skin['id']];state=bytes.fromhex(value['stateHex'])
            influences,count=struct.unpack_from('<II',state,9)
            assert influences==skin['blendInfluences']
            assert len(state)==17+count*64 and count==len(skin['edges'])-3
            assert value['edges']==skin['edges'],'actual native material/fog/mesh/bone identities'
            assert state[17:]==bytes.fromhex(skin['inverseBindHex']),'actual native inverse bind matrices byte-exact'
            bones+=count;rows.append(dict(case=case['name'],skinId=skin['id'],bones=count,nativePath=str(path),nativeSha256=sha(path)))
    report=dict(kind='original-pc-imported-skin-palette-comparison',status='passed',inputSha256=sha(source),
        cases=len(data['cases']),skins=len(rows),boneMatrixPairs=bones,inverseBindBytes=bones*64,rows=rows,
        scope='Full original loader output: exact skin identities, material/fog/base-mesh/bone edges, influence hint and inverse-bind bytes. Does not claim original rendering/gameplay.')
    Path(output_path).write_text(json.dumps(report,indent=2)+'\n',encoding='utf-8')
    print(json.dumps({k:v for k,v in report.items() if k!='rows'}))

if __name__=='__main__':main(*sys.argv[1:])
