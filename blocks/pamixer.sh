#!/bin/bash

case $BLOCK_BUTTON in
    1) pamixer --toggle-mute ;;
    3) setsid "$TERMINAL" -e pulsemixer & disown ;;
    4) pamixer --decrease 3 --allow-boost ;;
    5) pamixer --increase 3 --allow-boost ;;
    9) pamixer --decrease 1 --allow-boost ;;
   10) pamixer --increase 1 --allow-boost ;;
esac

read -r mute level <<< "$(pamixer --get-mute --get-volume)"
[ "$mute" = "false" ] && echo "🔊 $level" || echo "🔇 $level"
