#!/bin/dash

# Show 📡 and percent strength if connected to wifi
# Show 📶 if connected to ethernet
# Show ❎ if none.

case $BLOCK_BUTTON in
    1) notify-send "🌐 Network" "\
📡: wifi connection with quality
📶: ethernet connection
❎: no ethernet/wifi connection
❓: unknown ethernet status
" ;;
    3) setsid "$TERMINAL" -e nmtui & ;;
esac

if grep -q "down" /sys/class/net/w*/operstate ; then
    sed "s/down/❎/;s/up/📶/;s/unknown/❓/" /sys/class/net/e*/operstate
else
    grep "^\s*w" /proc/net/wireless | awk '{ print "📡", int($3 * 100 / 70) }'
fi
