#!/bin/bash
# ss.sh LABEL "EXTRA": Super Sprint in Flycast: coin, start, accelerate, steer
cd ${MAME4ALL:-../..}
source /opt/toolchains/dc/kos/environ.sh >/dev/null
rm -f src/naomi/*.o src/machine/atarisy2.o src/drivers/atarisy2.o
EXTRA="-DNAOMI_FLYCAST $2" naomi/build.sh ssprint 1 >/tmp/ss.log 2>&1 || { echo build failed; exit 1; }
cd ${FCTEST:-$(dirname "$0")}
FC_PEDALS=1 ./start.sh ${MAME4ALL:-../..}/mame4all_ssprint_naomi.bin >/dev/null
sleep 20; ./shot.sh s1
export DISPLAY=:99
sleep 10; ./shot.sh s2
xdotool keydown c; sleep 0.5; xdotool keyup c; sleep 2; ./shot.sh s3; xdotool keydown Return; sleep 0.5; xdotool keyup Return; sleep 6; ./shot.sh s4
xdotool keydown k; sleep 4; ./shot.sh s5; xdotool keydown l; sleep 1.5; xdotool keyup l; ./shot.sh s6; sleep 3; ./shot.sh s7; xdotool keydown j; sleep 1.5; xdotool keyup j; sleep 2; ./shot.sh s8; xdotool keyup k
convert s1.png s2.png s3.png s4.png +append r1.png; convert s5.png s6.png s7.png s8.png +append r2.png; convert r1.png r2.png -append -resize 1400x ss_$1.png
./stop.sh
