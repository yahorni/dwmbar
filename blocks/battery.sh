#!/bin/dash
set -e

capacity=$(cat /sys/class/power_supply/BAT0/capacity)
status=$(cat /sys/class/power_supply/BAT0/status)

if [ "$status" = "Charging" ]; then
    echo "🔌 $capacity"
elif [ "$status" = "Discharging" ] || [ "$status" = "Not charging" ] || [ "$status" = "Full" ]; then
    echo "🔋 $capacity"
else
    echo "❓ $capacity"
fi
