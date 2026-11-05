#!/bin/bash
# ssperf.sh LABEL "EXTRA": Super Sprint attract in Flycast, overlay crops
cd ${MAME4ALL:-../..}
source /opt/toolchains/dc/kos/environ.sh >/dev/null
rm -f src/naomi/*.o src/sdl/sound.o src/sndhrdw/pokey.o src/machine/atarisy2.o src/vidhrdw/atarisy2.o src/drivers/atarisy2.o
EXTRA="-DPROFILER_MAME4ALL $2" ./naomi/build.sh ssprint 1 >/tmp/ss.log 2>&1 || { echo build failed; exit 1; }
cd ${FCTEST:-$(dirname "$0")}
./start.sh ${MAME4ALL:-../..}/mame4all_ssprint_naomi.bin >/dev/null; sleep 20; L=""
for i in 1 2 3 4 5 6; do ./shot.sh a$i; convert a$i.png -crop 560x70+80+60 +repage a${i}c.png; L="$L a${i}c.png"; sleep 4; done
convert $L -append ssp_$1.png; ./stop.sh
