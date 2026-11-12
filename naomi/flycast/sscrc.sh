#!/bin/bash
# sscrc.sh LABEL "EXTRA": Super Sprint attract in Flycast for 50 s, overlay crops (RAM CRC row)
cd ${MAME4ALL:-../..}
source /opt/toolchains/dc/kos/environ.sh >/dev/null
rm -f src/naomi/*.o src/t11/t11.o src/sdl/video.o
EXTRA="-DPROFILER_MAME4ALL -DNAOMI_RAMCRC $2" ./naomi/build.sh ssprint 1 >/tmp/ss.log 2>&1 || { echo build failed; exit 1; }
cd ${FCTEST:-$(dirname "$0")}
./start.sh ${MAME4ALL:-../..}/mame4all_ssprint_naomi.bin >/dev/null; sleep 20; L=""
for i in 1 2 3 4 5 6 7 8; do sleep 5; ./shot.sh c$i; convert c$i.png -crop 600x80+80+60 +repage c${i}c.png; L="$L c${i}c.png"; done
convert $L -append crc_$1.png; ./stop.sh
