#!/usr/bin/env bash
set -e

read -r STATUS PERCENTAGE < <(acpi | tr -d ',' | tr -d '%' | cut -d ' ' -f3,4)

if [ "$STATUS" = "Charging" ]; then
    echo "🔌 $PERCENTAGE"
elif [ "$STATUS" = "Discharging" ] || [ "$STATUS" = "Full" ]; then
    echo -en "🔋 $PERCENTAGE"
else
    echo -en "❓ $PERCENTAGE"
fi
