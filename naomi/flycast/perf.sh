#!/bin/bash
# perf.sh LABEL "EXTRA defines" : build, run attract 20-34 s, crop the overlay
cd ${MAME4ALL:-../..}
rm -f src/*.o src/fame/*.o src/drivers/system16.o; MAKEOPTS="$3" EXTRA="$2" naomi/build.sh hangon 1 2>&1 | grep -E " error"
cd ${FCTEST:-$(dirname "$0")}
FC_PEDALS=1 ./start.sh ${MAME4ALL:-../..}/mame4all_hangon_naomi.bin >/dev/null
sleep 18
L=""
for i in 1 2 3 4 5 6 7; do ./shot.sh p$i; convert p$i.png -crop 400x56+80+60 +repage p${i}c.png; L="$L p${i}c.png"; sleep 2; done
convert $L -append perf_$1.png
