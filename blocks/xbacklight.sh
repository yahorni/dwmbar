#!/bin/dash

case $BLOCK_BUTTON in
    1)  backlight_dump="/tmp/backlight_dump"
        if [ "$(xbacklight -get)" -eq 1 ]; then
            [ -f "$backlight_dump" ] && xbacklight -set "$(cat $backlight_dump)"
        else
            xbacklight -get > $backlight_dump
            xbacklight -set 1
        fi
        ;;
    4)  xbacklight -dec 5 >/dev/null ;;
    5)  xbacklight -inc 5 >/dev/null ;;
esac

echo "🌞 $(xbacklight -get)"
