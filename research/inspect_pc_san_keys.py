#!/usr/bin/env python3
"""Bounded read-only PC SAN key/pool evidence; no game or database writes.

Single-object FFPS version 0x26 subset. Unknown fields are recorded, not silently
declared understood. This is not the general object-directory/serializer loader.
"""
from __future__ import annotations
import collections
import hashlib
import json
from pathlib import Path
import struct
import sys

ROOT=Path(__file__).resolve().parents[1]
DEFAULT=ROOT/'local-data/pc-pristine/Media/Animations'
MAX_FILE_BYTES=2*1024*1024

def u32(data,offset=0): return struct.unpack_from('<I',data,offset)[0]

def fields(data,offset):
    while offset<len(data):
        start=offset;raw=data[offset];offset+=1
        field=raw&31;size_code=raw>>5
        if field==31:
            if offset>=len(data):raise ValueError('truncated extended field ID')
            field=data[offset];offset+=1
        if size_code==0:
            # Native ReadHeader sets InvalidFieldID regardless of inline ID.
            # Requiring end-of-extent here is our strict inspection policy.
            if offset!=len(data):raise ValueError('unaccounted bytes after terminator')
            return
        if size_code<=4: size=(0,1,2,4,8)[size_code]
        else:
            width=(1,2,4)[size_code-5]
            if offset+width>len(data):raise ValueError('truncated explicit field length')
            size=int.from_bytes(data[offset:offset+width],'little');offset+=width
        if offset+size>len(data):raise ValueError('truncated field payload')
        yield field,data[offset:offset+size],start
        offset+=size
    raise ValueError('missing section terminator')

def key_payload(payload,role):
    result=[];offset=0;axis_count=1
    while len(result)<axis_count:
        rep=u32(payload,offset);offset+=4
        if rep==0:
            if offset!=len(payload):raise ValueError('trailing empty-curve bytes')
            return []
        if rep not in (1,2,3,4):raise ValueError(f'unknown key representation {rep}')
        if not result and rep>=3:axis_count=3
        count=u32(payload,offset);offset+=4
        stride=5 if rep==4 else 1 if rep==3 else (8 if role==1 else 15) if rep==2 else (4 if role==1 else 3)
        if count>MAX_FILE_BYTES//4 or offset+count*(4+stride*4)>len(payload):
            raise ValueError('invalid key count/payload size')
        times=struct.unpack_from('<'+'f'*count,payload,offset);offset+=count*4
        values=struct.unpack_from('<'+'f'*(count*stride),payload,offset);offset+=count*stride*4
        result.append({'representation':rep,'count':count,'times':times,'values':values})
    if offset!=len(payload):raise ValueError('unaccounted key bytes')
    return result

def inspect(path):
    if path.stat().st_size>MAX_FILE_BYTES:raise ValueError('file exceeds bounded probe limit')
    raw=path.read_bytes()
    if raw[:4]!=b'FFPS' or u32(raw,4)!=0x26 or u32(raw,12)!=len(raw) or u32(raw,28)!=1:
        raise ValueError('not the supported single-object PC SAN envelope')
    start,size=u32(raw,20),u32(raw,24)
    data=raw[start:start+size]
    if start+size!=len(raw) or data[:8]!=bytes.fromhex('3a56ee5653424f4f'):
        raise ValueError('not a complete spAnimation payload')
    pools={};used=[0]*7;tracks=[];pending={};unknown=[];duration=None
    for kind,payload,offset in fields(data,8):
        if kind==0:
            if payload:duration=struct.unpack('<f',payload)[0]
        elif kind in range(6,13) or kind==64:
            if len(payload)!=4:raise ValueError('pool/count field size')
            pools[kind]=u32(payload)
        elif kind in (2,3,4):
            role=kind-2;channels=key_payload(payload,role)
            pending[role]={'channels':channels,'payload':payload,'offset':start+offset}
            for channel in channels:
                rep,count=channel['representation'],channel['count']
                pool=0 if rep==3 else 1 if rep==4 else 4 if role==1 and rep==1 else 5 if role==1 else 2 if rep==1 else 3
                used[pool]+=count;used[6]+=count
        elif kind==1:
            length=struct.unpack_from('<H',payload)[0]
            if length+2!=len(payload):raise ValueError('track name extent')
            tracks.append({'name':payload[2:].decode('latin1').rstrip('\0'),'roles':pending})
            pending={}
        else:unknown.append({'field':kind,'size':len(payload),'offset':start+offset})
    if pending:raise ValueError('unterminated track')
    summary={'path':str(path.relative_to(ROOT)),'sha256':hashlib.sha256(raw).hexdigest().upper(),
             'duration':duration,'tracks':len(tracks),'track_reserve_hint':pools.get(64),
             'declared_pools':[pools.get(i) for i in range(6,13)],'observed_pools':used,
             'unknown_fields':unknown,'key_shapes':dict(collections.Counter(
                 f'role{role}/repr{channel["representation"]}/count{channel["count"]}'
                 for track in tracks for role,data in track['roles'].items() for channel in data['channels']))}
    summary['pool_hints_match']=all(pools.get(i+6,0)==used[i] for i in range(7))
    summary['pool_hints_cover']=all(pools.get(i+6,0)>=used[i] for i in range(7))
    # Field 64 is optional reservation, not the actual completed-track count.
    # Pool mismatches are observations here, not a fabricated native rejection.
    return summary,tracks

def main():
    paths=[Path(s) for s in sys.argv[1:]] or [DEFAULT/name for name in ('barrel.san','bbush.san','bflower.san','bw.san')]
    if len(paths)>12:raise ValueError('at most 12 files per read-only probe')
    for path in paths:print(json.dumps(inspect(path)[0],ensure_ascii=True))
    return 0

if __name__=='__main__':raise SystemExit(main())
