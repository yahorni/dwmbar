#!/usr/bin/env dash
set -e

case $BLOCK_BUTTON in
    1) xkb-switch -n ;;
    3) xboard.sh ;;
esac

echo "🌍 $(xkb-switch)"
