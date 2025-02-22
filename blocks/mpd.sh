#!/bin/bash

handle_block_button() {
    case "$BLOCK_BUTTON" in
        1) mpc toggle ;;
        2) mpc stop ;;
        3) setsid "$TERMINAL" -e ncmpcpp & ;;
        4) mpc prev ;;
        5) mpc next ;;
        6) parse_metadata ; send_notification ;;
    esac >/dev/null 2>&1
}

handle_status() {
    case "$status" in
        playing)
            status_icon='⏸'
            message="$(get_current_song)"
            ;;
        paused)
            status_icon='▶'
            message="$(get_current_song)"
            ;;
        stopped)
            status_icon='⏹'
            message="$status"
            ;;
        *)
            status_icon='❓'
            message="$status"
            ;;
    esac
    echo "🎵 $message $status_icon"
}

parse_metadata() {
    [ -n "$status" ] && return
    status="$(mpc status '%state%' | head -n1)"
    IFS='' read -r artist title duration <<< \
        "$(mpc status -f "%artist%%title%%time%" | head -n1)"
}

get_current_song() {
    if [ -n "$title" ]; then
        title_length_max=40
        [ "${#title}" -gt $title_length_max ] &&\
            title="${title:0:$((title_length_max - 3))}..."

        if [ -n "$artist" ]; then
            echo "$artist — $title [$duration]"
        else
            echo "$title [$duration]"
        fi
    else
        echo "$(basename "$(mpc status -f "%file%" | head -n1)") [$duration]"
    fi
}

send_notification() {
    local message
    if [ "$status" = "playing" ] || [ "$status" = "paused" ]; then
        position="$(mpc status '%currenttime%')/$duration"

        message="${artist:-\[Unknown Artist\]}\n"
        if [ -n "$title" ]; then
            message="$message<i>$title</i>\n"
        else
            message="$message<i>$(basename "$(mpc status -f "%file%" | head -n1)")</i>\n"
        fi
        message="${message}[$position]"
    else
        message="stopped"
    fi
    notify-send -r 10000 "🎵 MPD" "$message"
}

## main
declare artist title duration status
handle_block_button
parse_metadata
handle_status
