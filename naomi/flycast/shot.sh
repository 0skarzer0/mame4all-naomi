#!/bin/bash
# shot.sh name
export DISPLAY=:99
import -window root ${FCTEST:-$(dirname "$0")}/$1.png; convert ${FCTEST:-$(dirname "$0")}/$1.png -resize 400x300 ${FCTEST:-$(dirname "$0")}/$1_s.png
