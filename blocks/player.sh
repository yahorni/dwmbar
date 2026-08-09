#!/usr/bin/env bash
set -euo pipefail

# type: playerctl or mpc
player_type="playerctl"
line_limit=35

handle_block_button_playerctl() {
    case "${BLOCK_BUTTON:-}" in
        1) playerctl play-pause ;;
        2) playerctl -a pause ;;
        3) switch_player ;;
        4) playerctl previous ;;
        5) playerctl next ;;
    esac >/dev/null 2>&1
}

handle_block_button_mpc() {
    case "${BLOCK_BUTTON:-}" in
        1) mpc toggle ;;
        2) mpc stop ;;
        3) setsid "$TERMINAL" -e ncmpcpp & ;;
        4) mpc prev ;;
        5) mpc next ;;
    esac >/dev/null 2>&1
}

switch_player() {
    local player="$(playerctl -l | rofi -dmenu -p "Select player" || :)"
    if [ -n "$player" ]; then
        playerctl -a pause
        playerctl --player="$player" play
    fi
}

parse_metadata_playerctl() {
    status="$(playerctl status 2>&1 | tr '[:upper:]' '[:lower:]' || true)"
    IFS='' read -r artist title length <<< \
        "$(playerctl metadata -f "{{ artist }}{{ title }}{{ duration(mpris:length) }}" 2>/dev/null)"
}

parse_metadata_mpc() {
    status="$(mpc status '%state%' | head -n1)"
    IFS='' read -r artist title length file <<< \
        "$(mpc status -f "%artist%%title%%time%%file%" | head -n1)"
}

shorten_line_if_too_long() {
    if [ "${#1}" -gt "$line_limit" ]; then
        echo -n "${1:0:$((line_limit - 3))}..."
    else
        echo -n "$1"
    fi
}

get_current_song() {
    title="$(shorten_line_if_too_long "$title")"
    artist="$(shorten_line_if_too_long "$artist")"

    local song=""
    if [ -n "$title" ]; then
        if [ -n "$artist" ]; then
            song="$artist — $title"
        else
            song="$title"
        fi
    elif [ -n "$file" ]; then
        song="$(shorten_line_if_too_long "$(basename "$(echo -n "$file" | head -n1)")")"
    fi
    if [ -n "$length" ]; then
        song="$song [$length]"
    fi

    echo -n "$song"
}

handle_status() {
    local status_icon message
    case "${status:-}" in
        playing)
            status_icon='▶'
            message="$(get_current_song)"
            ;;
        paused)
            status_icon='⏸'
            message="$(get_current_song)"
            ;;
        stopped|no\ player*)
            status_icon='⏹'
            message="$status"
            ;;
        *)
            status_icon='❓'
            message="${status:-}"
            ;;
    esac
    echo -n "🎵 $message $status_icon"
}

### main

declare status artist title length file=""
case "$player_type" in
    mpc)
        handle_block_button_mpc
        parse_metadata_mpc
        ;;
    playerctl)
        handle_block_button_playerctl
        parse_metadata_playerctl
        ;;
    *)
        echo -n "unsupported player: $player_type"
        exit 1
        ;;
esac
handle_status
