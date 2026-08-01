#!/usr/bin/env dash
set -e

case "$BLOCK_BUTTON" in
    1)  brillo -S 50 ;;
    3)  brillo -S 100 ;;
    4)  brillo -U 1 -q ;;
    5)  brillo -A 1 -q ;;
    9)  brillo -U 1 ;;
    10) brillo -A 1 ;;
esac

echo "🌞 $(brillo | cut -d'.' -f1)"
