#!/usr/bin/env dash
set -e

cmd="pamixer --default-source --get-mute --get-volume"

case "$BLOCK_BUTTON" in
    1) status=$($cmd --toggle-mute) ;;
    3) status=$($cmd) ; setsid "$TERMINAL" -e pulsemixer >/dev/null 2>&1 & ;;
    4) status=$($cmd --decrease 5) ;;
    5) status=$($cmd --increase 5) ;;
    9) status=$($cmd --decrease 1) ;;
   10) status=$($cmd --increase 1) ;;
    *) status=$($cmd) ;;
esac

level=$(echo "$status" | cut -d' ' -f2)
case "$status" in
    false*) echo "🎤 🔴 $level" ;;
    true*)  echo "🎤 $level" ;;
esac
