#!/bin/bash
# kgt.sh LABEL "EXTRA" "MAKEOPTS": Konami GT in Flycast: coin, start, accelerate, steer
cd ${MAME4ALL:-../..}
source /opt/toolchains/dc/kos/environ.sh >/dev/null
rm -f src/fame/m68000.o src/drivers/nemesis.o
MAKEOPTS="$3" EXTRA="-DNAOMI_FLYCAST $2" naomi/build.sh konamigt 1 >/tmp/k.log 2>&1 || { echo build failed; exit 1; }
cd ${FCTEST:-$(dirname "$0")}
FC_PEDALS=1 ./start.sh ${MAME4ALL:-../..}/mame4all_konamigt_naomi.bin >/dev/null
sleep 30
export DISPLAY=:99
xdotool keydown c; sleep 0.5; xdotool keyup c; sleep 2; xdotool keydown Return; sleep 0.5; xdotool keyup Return; sleep 6; ./shot.sh k1
xdotool keydown k; sleep 8; ./shot.sh k2; xdotool keydown l; sleep 2; ./shot.sh k3; xdotool keyup l; sleep 5; ./shot.sh k4; xdotool keyup k
convert k1.png k2.png +append r1.png; convert k3.png k4.png +append r2.png; convert r1.png r2.png -append -resize 1000x kgt_$1.png
./stop.sh
