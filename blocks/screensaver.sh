#!/usr/bin/env dash
set -e

is_enabled() {
    local xset_output=$(xset q)
    local screensaver_timeout=$(echo "$xset_output" | grep -E '^[[:space:]]+timeout:' | awk '{print $2}')
    local dpms_enabled=$(echo "$xset_output" | grep -c "DPMS is Enabled")
    [ "$screensaver_timeout" -gt 0 ] && [ "$dpms_enabled" -eq 1 ]
}

toggle() {
    if is_enabled; then
        xset s off -dpms
    else
        xset s on +dpms
    fi
}

case "$BLOCK_BUTTON" in
    1) toggle ;;
esac

printf "☕ "
if is_enabled; then
    printf "on"
else
    printf "off"
fi
