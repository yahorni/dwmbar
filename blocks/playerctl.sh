#!/bin/bash

handle_block_button() {
    case "$BLOCK_BUTTON" in
        1) playerctl play-pause ;;
        2) playerctl -a pause ;;
        3) setsid supersonic-desktop & ;;
        4) playerctl previous ;;
        5) playerctl next ;;
        6) parse_metadata ; send_notification ;;
        9) playerctld shift ;;
        10) playerctld unshift ;;
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
        stopped|no\ player*)
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
    status="$(playerctl status 2>&1 | tr '[:upper:]' '[:lower:]')"
    IFS='' read -r artist title duration <<< \
        "$(playerctl metadata -f "{{ artist }}{{ title }}{{ duration(mpris:length) }}")"
}

get_current_song() {
    local song
    if [ -n "$title" ]; then
        local title_length_max=40
        [ "${#title}" -gt $title_length_max ] &&\
            title="${title:0:$((title_length_max - 3))}..."

        if [ -n "$artist" ]; then
            song="$artist — $title"
        else
            song="$title"
        fi
    fi
    if [ -n "$duration" ]; then
        song="$song [$duration]"
    fi
    echo "$song"
}

send_notification() {
    local message
    if [ "$status" = "playing" ] || [ "$status" = "paused" ]; then
        if [ -n "$artist" ]; then
            message="$artist\n"
        fi
        message="$message<i>$title</i>"
        if [ -n "$duration" ]; then
            position="$(playerctl metadata -f "{{ duration(position) }}")/$duration"
            message="$message\n[$position]"
        fi
    else
        message="stopped"
    fi
    notify-send -r 10000 "🎵 Playerctl" "$message"
}

## main
declare artist title duration status
handle_block_button
parse_metadata
handle_status
