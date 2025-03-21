#!/bin/bash

case $BLOCK_BUTTON in
    1) amixer set Master toggle >/dev/null ;;
    3) setsid "$TERMINAL" -e alsamixer & disown ;;
    4) amixer set Master 5%- >/dev/null ;;
    5) amixer set Master 5%+ >/dev/null ;;
esac

read -r level stat < <(amixer get Master | \
    tail -n1 | \
    tr -d "[]%" | \
    sed -e 's/^[[:space:]]*//' -e 's/[[:space:]]*$//' | \
    cut -d ' ' -f4,6)

[[ $stat = "on" ]] && echo "🔊 $level" || echo "🔇 $level%"
