#!/bin/bash
# run.sh ROM SECONDS [shot step]   env: KEYS="t:key[:hold],..."  DYNAREC=yes|no  FC_WATCH=lo-hi
ROM=$1; SECS=${2:-30}; STEP=${3:-5}
cd ${FCTEST:-$(dirname "$0")}
rm -rf shots; mkdir -p shots
pkill -x Xvfb 2>/dev/null; sleep 0.5
Xvfb :99 -screen 0 800x600x24 >/dev/null 2>&1 &
XP=$!
sleep 1
export DISPLAY=:99 SDL_AUDIODRIVER=dummy LIBGL_ALWAYS_SOFTWARE=1 HOME=${FCTEST:-$(dirname "$0")}/home
timeout $((SECS+10)) ${FLYCAST:-../flycast}/build/flycast -config config:UseReios=yes -config config:Dynarec.Enabled=${DYNAREC:-yes} \
  -config log:LogToConsole=yes -config log:Verbosity=3 "$ROM" > log.txt 2>&1 &
FP=$!
IFS=',' read -ra EV <<< "${KEYS:-}"
for ((t=1; t<=SECS; t++)); do
  sleep 1
  for e in "${EV[@]}"; do
    IFS=':' read -r et ek eh <<< "$e"
    if [ "$et" == "$t" ]; then xdotool keydown $ek; sleep ${eh:-0.2}; xdotool keyup $ek; fi
  done
  if (( t % STEP == 0 )); then import -window root shots/t$(printf %03d $t).png 2>/dev/null; fi
done
rm -f /tmp/fc_dump_done; touch /tmp/fc_dump_now; for i in 1 2 3 4 5 6; do [ -f /tmp/fc_dump_done ] && break; sleep 1; done; kill $FP 2>/dev/null; sleep 1; kill -9 $FP 2>/dev/null; kill $XP 2>/dev/null
