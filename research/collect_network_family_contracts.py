#!/usr/bin/env python3
"""Seal bounded PC network lifetimes and the original packet/stream codec."""
import hashlib
import json
import struct
from pathlib import Path
from capture_native_ranges import ROOT, PC, EXPECTED, read_window, pefile


def main():
    p = ROOT / 'local-data/results/native-cycle-20260910-1900/network-family'
    out = ROOT / 'research/network-family-contracts-2026-09-10.json'
    if out.exists():
        raise ValueError('Sealed evidence is immutable')
    def sha(a): return hashlib.sha256(a.read_bytes()).hexdigest().upper()
    def ref(a): return dict(path=a.relative_to(ROOT).as_posix(), sha256=sha(a))
    raw = PC.read_bytes()
    assert hashlib.sha256(raw).hexdigest().upper() == EXPECTED['pc']
    pe = pefile.PE(data=raw, fast_load=True)
    tables = dict(spDXNetwork=0x6ef90c, spNetwork=0x6eee30,
                  spNetworkDebug=0x6ebbb8, spNetworkManager=0x6e6b3c,
                  spNetworkMatchmaking=0x6ebcd0, spNetworkPacket=0x6ed154,
                  spNetworkPeer=0x6ebd7c, spNetworkServer=0x6ec04c,
                  spNetworkStateCtrl=0x6ebd3c, spSocketStream=0x6ed0a8)
    runs = dict(spDXNetwork=4, spNetworkDebug=4, spNetworkManager=2,
                spNetworkMatchmaking=3, spNetworkPacket=1, spNetworkPeer=2,
                spNetworkServer=2, spNetworkStateCtrl=2, spSocketStream=2)
    versions = [p/'source-snapshot/probe_pc_ai_action_lifecycle.py'] + list(p.glob('probe-pc-lifecycle-*.py'))
    by_sha = {sha(a): a for a in versions}
    all_runs = []
    for a in sorted(p.glob('pc-sp*-run*.json')):
        r = json.loads(a.read_text())
        assert r['sourceSha256'] in by_sha, a
        all_runs.append(dict(report=ref(a), exactRunner=ref(by_sha[r['sourceSha256']]), status=r['status']))
    rows = []
    for row in json.loads((p/'catalog-family.json').read_text()):
        n = row['className']
        assert row['ps2Record'] is None and row['ps2Factory'] is None
        a = tables[n]
        b, off = read_window('pc', raw, a, 28, pe)
        slots = struct.unpack('<7I', b)
        getter, goff = read_window('pc', raw, slots[4], 6, pe)
        assert getter == b'\xb8' + struct.pack('<I', row['pcRecord']) + b'\xc3'
        item = dict(row, pc=dict(objectVtable=f'{a:08X}', objectVtableBytes=b.hex(),
                    objectVtableFileOffset=off, objectSlots=[f'{v:08X}' for v in slots],
                    getterBytes=getter.hex(), getterFileOffset=goff,
                    objectInterfaceOffset=4 if n in ('spNetwork','spDXNetwork') else 0),
                    ps2=dict(status='not-in-registered-class-catalog', scope='No PS2 counterpart or implementation inferred'))
        if n in ('spNetwork', 'spDXNetwork'):
            primary = 0x6eee50 if n == 'spNetwork' else 0x6ef928
            b, off = read_window('pc', raw, primary, 28, pe)
            item['pc'].update(primaryVtable=f'{primary:08X}', primaryBytes=b.hex(), primaryFileOffset=off)
        if n in runs:
            a = p / f'pc-{n}-run{runs[n]}.json'
            r = json.loads(a.read_text())
            assert r['status'] == 'passed' and len(r['stages']) == 5
            assert r['initial']['slots'] == item['pc']['objectSlots']
            item['pc'].update(lifetimeSource=ref(a), exactRunner=ref(by_sha[r['sourceSha256']]),
                              allocationBytes=r['initial']['allocationBytes'],
                              initialBytes=r['initial']['bytes'], cloneBytes=r['clone']['bytes'],
                              platformInput=r.get('platformInput'), crtFixture=r['crtFixture'],
                              remainingAllocations=r['remainingAllocations'])
        else:
            assert slots[2] == 0x4a1bf0 and row['pcFactory'] == 0
            item['pc']['scope'] = 'Metadata, getter, null Clone and secondary destructor+4; no fabricated base factory'
        rows.append(item)
    packet = json.loads((p/'packet-io-run1.json').read_text())
    assert packet['status'] == 'passed' and len(packet['cases']) == 15
    assert packet['sourceSha256'] == sha(ROOT/'research/probe_network_packet_io.py')
    sources = [p/n for n in ('catalog-family.json','factories/capture.json',
               'getter-candidates-run1.json','initial-protocol/capture.json',
               'interface-offset/capture.json','base-interface-proof/capture.json','packet-io-run1.json')]
    report = dict(kind='pc-network-family-native-contracts', schemaVersion=1, inputs=EXPECTED,
                  collectorSha256=sha(Path(__file__)), classes=rows, sources=[ref(a) for a in sources],
                  allLifecycleRuns=all_runs,
                  counts=dict(pcClasses=10, ps2RegisteredCounterparts=0, pcLifetimes=9,
                              pcClassOperations=45, startupFailureLifetimes=5, packetCases=15),
                  packet=dict(objectBytes=40, payloadAllocationBytes=256, wireHeaderBytes=17,
                              wireFormat='<HHHBIIH', reader='004996D0', writer='00499850',
                              fields=['Source:uint16@10','Destination:uint16@12','PacketType:uint16@14',
                                      'UseTcp:uint8@16','PacketDataField1:uint32@18','PacketDataField2:uint32@1C','Size:uint16@20'],
                              factory='00499960', clone='004999C0', payloadPointerOffset='24',
                              memoryStreamFactory='00465560', memoryStreamBytes=56,
                              cloneBehavior='Fresh packet header and separate256-byte payload; contents not copied',
                              defaults='Source/Destination BAD0,data1/data2/size zero;type/TCP retain allocation poison',
                              scope=packet['scope']),
                  limitsAndCorrections=[
                      'Five lifetimes use literal WSAStartup10091 without WSADATA writes and WSACleanupFFFFFFFF. Actual socket startup/connect/send/receive were not supplied or exercised.',
                      'CRT char_traits reuses the existing bounded fixture. Only four existing single-thread critical-section inputs are selected; this does not prove concurrency.',
                      'NetworkDebug executes its own embedded spTimer against literal timeGetTime1200. Old global-only TimerFixture refresh seams were removed for this explicit profile.',
                      'DXNetwork run2 failed at RTTI by reading its primary network interface as BaseObject. Native secondary table+4 and Clone return+4 are used from run3. Run3 then exposed WSACleanup, supplied explicitly in run4. No game callback was forced successful.',
                      'Window named pc-network-base-construction at465CE0 is the protected DX constructor path calling base4A1C40. Base secondary+4 independently follows from actual4A1C00 destructor and4A1C30 adjustor; no linear protected-junk disassembly is accepted as executed code.',
                      'Packet truncated-header cases except empty stop before the next missing scalar read. Size257 cases stop before raw payload transfer; caller-side size policy and complete logged failure paths remain open.',
                      'No corresponding PS2 registered classes found in the cached complete catalog. This does not prove there is no unregistered networking code.'
                  ])
    out.write_text(json.dumps(report, indent=2)+'\n', encoding='utf-8')
    print(json.dumps(report['counts']))


if __name__ == '__main__':
    main()
