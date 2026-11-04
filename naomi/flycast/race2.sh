#!/bin/bash
cd ${FCTEST:-$(dirname "$0")}
FC_PEDALS=1 ./start.sh ${MAME4ALL:-../..}/mame4all_hangon_naomi.bin >/dev/null
sleep 14
export DISPLAY=:99
xdotool keydown c; sleep 0.5; xdotool keyup c; sleep 1.5; xdotool keydown Return; sleep 0.5; xdotool keyup Return
sleep 2; ./shot.sh h1; sleep 3; ./shot.sh h2; sleep 3; ./shot.sh h3
xdotool keydown j; sleep 3; ./shot.sh h4; xdotool keyup j
convert h1.png h2.png +append r1.png; convert h3.png h4.png +append r2.png; convert r1.png r2.png -append -resize 1000x race2.png
