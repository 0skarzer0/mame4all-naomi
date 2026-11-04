#!/bin/bash
# naomi/build.sh GAME [FPS]   ->  mame4all_GAME_naomi.bin (netboot image)
set -e
cd "$(dirname "$0")/.."
source /opt/toolchains/dc/kos/environ.sh
GAME=${1:-hangon}
FPS=${2:-0}
# objects depend on the game (driver flags): rebuild all when it changes
if [ "$(cat .naomi_last 2>/dev/null)" != "$GAME $EXTRA $MAKEOPTS" ]; then
	make -f Makefile.naomi clean >/dev/null
	echo "$GAME $EXTRA $MAKEOPTS" > .naomi_last
fi
rm -f src/naomi/*.o src/sdl/video.o
# the romdisk holds only this game's ROM zip (naomi/roms/GAME.zip, made by
# naomi/tools/romzip.py; never committed)
[ -f naomi/roms/$GAME.zip ] || { echo "naomi/roms/$GAME.zip missing"; exit 1; }
rm -f naomi/romdisk/roms/*.zip romdisk.img romdisk.o
cp naomi/roms/$GAME.zip naomi/romdisk/roms/
make -f Makefile.naomi GAME=$GAME FPS=$FPS EXTRA="$EXTRA" $MAKEOPTS -j8
TITLE=$(echo "$GAME" | tr a-z A-Z)
$KOS_BASE/utils/naomibintool/naomibintool build mame4all_${GAME}_naomi.bin -t "$TITLE MAME4ALL" -d 0SKARZER0 -e mame4all_$GAME.elf
