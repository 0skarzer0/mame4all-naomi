#!/bin/bash
# start.sh ROM - headless Flycast left running; use key.sh / shot.sh / stop.sh
cd ${FCTEST:-$(dirname "$0")}
pkill -x flycast 2>/dev/null; pkill -x Xvfb 2>/dev/null; sleep 0.5
Xvfb :99 -screen 0 800x600x24 >/dev/null 2>&1 &
sleep 1
export FC_MIE_NODEVINFO=1 DISPLAY=:99 SDL_AUDIODRIVER=${AUDIODRV:-dummy} SDL_DISKAUDIOFILE=${FCTEST:-$(dirname "$0")}/audio.raw LIBGL_ALWAYS_SOFTWARE=1 HOME=${FCTEST:-$(dirname "$0")}/home
nohup ${FLYCAST:-../flycast}/build/flycast -config config:UseReios=yes -config config:Dynarec.Enabled=${DYNAREC:-yes} \
  -config Debug:SerialConsoleEnabled=yes -config log:LogToConsole=yes -config log:Verbosity=3 "$1" > log.txt 2>&1 &
echo started
