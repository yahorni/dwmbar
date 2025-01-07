#!/bin/dash

case $BLOCK_BUTTON in
    1)  brillo -q -S 75 ;;
    3)  brillo -q -S 100 ;;
    4)  brillo -q -U 3 ;;
    5)  brillo -q -A 3 ;;
esac

echo "🌞 $(brillo | cut -d'.' -f1)"
