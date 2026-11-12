#!/usr/bin/env python3
"""live.py NETDIMM_IP ADDR [SECONDS]: live figures of a mame4all-naomi image through the NetDIMM.

Runs ON the netbooter Raspberry (WiPi), with DragonMinded's netdimm.py in /sbin/piforce/dm_netboot, while the
game runs (src/naomi/naomi_dimmcomms.cpp answers the peeks). ADDR is the address of `naomi_live`:
`sh-elf-nm mame4all_GAME.elf | grep naomi_live`. Same idea as live.py of the Daytona USA NAOMI port.
Times are tenths of a ms per emulated frame (upload: per frame shown).

Example: python3 live.py 10.0.0.4 0x8c3a1b40 60
"""
import sys, time
sys.path.append('/sbin/piforce/dm_netboot')
from netdimm import NetDimm, PeekPokeTypeEnum as T

FIELDS = ('magic version seconds emulated shown busy game_loops cpu0 cpu1 cpu2 sound video upload sound_out '
          'dimm_requests emulated_worst').split()


def main():
    if len(sys.argv) < 3:
        sys.exit(__doc__)
    dimm, base = NetDimm(sys.argv[1]), int(sys.argv[2], 0)
    seconds = int(sys.argv[3]) if len(sys.argv) > 3 else 30
    peek = lambda a: dimm.peek(a & 0x01ffffff, T.TYPE_LONG)
    read = lambda: dict(zip(FIELDS, (peek(base + 4 * i) for i in range(len(FIELDS)))))
    live = read()
    if live['magic'] != 0x454d414d:
        sys.exit('no live block at 0x%08x (magic %08x): wrong address, or the game is not running' % (base, live['magic']))
    last = None
    t = lambda v: v / 10
    for _ in range(seconds * 4):
        live = read()
        if live['seconds'] != last:
            last = live['seconds']
            print('%4ds emu %2d shown %2d busy %3d%% loops %2d | cpu0 %4.1f cpu1 %4.1f cpu2 %4.1f snd %4.1f vid %4.1f '
                  'upl %4.1f | worst %2d' % (live['seconds'], live['emulated'], live['shown'], live['busy'],
                                              live['game_loops'], t(live['cpu0']), t(live['cpu1']), t(live['cpu2']),
                                              t(live['sound']), t(live['video']), t(live['upload']),
                                              live['emulated_worst']), flush=True)
        time.sleep(0.25)


if __name__ == '__main__':
    main()
