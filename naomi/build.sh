#!/bin/bash
# naomi/build.sh GAME [FPS]   ->  mame4all_GAME_naomi.bin (netboot image)
set -e
cd "$(dirname "$0")/.."
source /opt/toolchains/dc/kos/environ.sh
GAME=${1:-hangon}
FPS=${2:-0}
# objects depend on the game (driver flags): rebuild all when it changes
if [ "$(cat .naomi_last 2>/dev/null)" != "$GAME $EXTRA $MAKEOPTS" ]; then
	make -f Makefile.naomi GAME=$GAME clean >/dev/null
	echo "$GAME $EXTRA $MAKEOPTS" > .naomi_last
fi
rm -f src/naomi/*.o src/sdl/video.o
# the romdisk holds only this game's ROM zip (naomi/roms/GAME.zip, made by
# naomi/tools/romzip.py; never committed)
[ -f naomi/roms/$GAME.zip ] || { echo "naomi/roms/$GAME.zip missing"; exit 1; }
rm -f naomi/romdisk/roms/*.zip romdisk.img romdisk.o
cp naomi/roms/$GAME.zip naomi/romdisk/roms/
# Super Sprint: the T11 program is statically recompiled from the user's ROMs
# (src/t11/t11_ssprint_rc.h, generated here and never committed)
if [ "$GAME" = ssprint ]; then
	T=$(mktemp -d)
	unzip -o -q -j naomi/roms/ssprint.zip 136042.138 136042.139 -d "$T"
	python3 naomi/tools/t11recomp.py src/t11/t11ops.h src/t11/t11table.h \
		"$T/136042.138" "$T/136042.139" src/t11/t11_ssprint_rc.h naomi/tools/ssprint_t11_entries.txt
	rm -rf "$T"
	rm -f src/t11/t11.o
fi
make -f Makefile.naomi GAME=$GAME FPS=$FPS EXTRA="$EXTRA" $MAKEOPTS -j8
TITLE=$(echo "$GAME" | tr a-z A-Z)
$KOS_BASE/utils/naomibintool/naomibintool build mame4all_${GAME}_naomi.bin -t "$TITLE MAME4ALL" -d 0SKARZER0 -e mame4all_$GAME.elf
