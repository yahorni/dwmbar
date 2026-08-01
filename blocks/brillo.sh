#!/usr/bin/env dash
set -e

case "$BLOCK_BUTTON" in
    1) brillo -S 75 ;;
    3) brillo -S 100 ;;
    4) brillo -q -U 1 ;;
    5) brillo -q -A 1 ;;
esac

echo "🌞 $(brillo | cut -d'.' -f1)"
