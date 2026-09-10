"""Explicit timing and selected startup-failure inputs; no host networking."""


def install_network_platform_inputs(p,profile):
    if profile not in ('network-clock','network-startup-failure'):raise ValueError('Explicit network platform profile required')
    base=0x34170000;p.mu.mem_map(base,4096,p.uc.UC_PROT_READ|p.uc.UC_PROT_EXEC);events=[]
    # Execute the original spTimer Start/Stop methods for embedded timers too.
    # Only the verified WINMM.timeGetTime import supplies a literal timestamp.
    for address in (0x6be2d0,0x6be2f0):p.seams.pop(address,None)
    def clock(m):events.append(dict(importName='WINMM.timeGetTime',result=1200));m.fixture_return(eax=1200)
    p.put_uint(0x6d9454,base+16);p.seams[base+16]=clock
    if profile=='network-startup-failure':
        def startup(m):
            sp=m.reg('ESP');version=m.uint(sp+4);out=m.uint(sp+8)
            # Chosen nonzero failure input; WSADATA is deliberately untouched.
            # This does not claim a successful socket subsystem or connection.
            events.append(dict(importName='WS2_32.WSAStartup',requestedVersion=version,out=out,result=10091,outputWritten=False))
            m.fixture_return(8,eax=10091)
        p.put_uint(0x6d9490,base+32);p.seams[base+32]=startup
        def cleanup(m):
            events.append(dict(importName='WS2_32.WSACleanup',result=0xffffffff,scope='chosen failure after failed startup;no host cleanup'))
            m.fixture_return(eax=0xffffffff)
        p.put_uint(0x6d94d8,base+48);p.seams[base+48]=cleanup
    return events
