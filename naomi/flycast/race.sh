#!/bin/bash
# race.sh LABEL "EXTRA": build (Flycast input), coin, start, hold accel; grid race_LABEL.png
cd ${MAME4ALL:-../..}
source /opt/toolchains/dc/kos/environ.sh
rm -f src/drivers/system16.o src/fame/m68000.o
MAKEOPTS="$3" EXTRA="-DNAOMI_FLYCAST $2" naomi/build.sh hangon 1 2>&1 | grep -E " error|undefined"
cd ${FCTEST:-$(dirname "$0")}
FC_PEDALS=1 ./start.sh ${MAME4ALL:-../..}/mame4all_hangon_naomi.bin >/dev/null
sleep 14
export DISPLAY=:99
xdotool keydown c; sleep 0.5; xdotool keyup c; sleep 1.5; xdotool keydown Return; sleep 0.5; xdotool keyup Return; sleep 3; ./shot.sh g1
xdotool keydown k; sleep 6; ./shot.sh g2; sleep 4; ./shot.sh g3; xdotool keydown l; sleep 1.5; ./shot.sh g4; xdotool keyup l; xdotool keyup k
convert g1.png g2.png +append r1.png; convert g3.png g4.png +append r2.png; convert r1.png r2.png -append -resize 1000x race_$1.png
