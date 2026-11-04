#!/bin/bash
# key.sh key [hold seconds] [repeat]
export DISPLAY=:99
for ((i=0;i<${3:-1};i++)); do xdotool keydown $1; sleep ${2:-0.3}; xdotool keyup $1; sleep 0.5; done
