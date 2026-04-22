#!/usr/bin/env dash
set -e

notify_id=8432
case "$BLOCK_BUTTON" in
    1)  if systemctl is-active tailscaled ; then
            if tailscale status ; then
                status_msg="IPv4 $(tailscale ip -4)\n$(grep nameserver /etc/resolv.conf)"
            else
                status_msg="Client stopped"
            fi
        else
            status_msg="Daemon stopped"
        fi
        notify-send -r "$notify_id" "👾 Tailscale" "$status_msg"
        ;;
    2)  if ! systemctl is-active tailscaled ; then
            notify-send -r "$notify_id" "👾 Tailscale" "Starting daemon"
            sudo -A systemctl start tailscaled
        elif tailscale status ; then
            notify-send -r "$notify_id" "👾 Tailscale" "Stopping client"
            sudo -A tailscale down
        fi
        notify-send -r "$notify_id" "👾 Tailscale" "Starting client"
        sudo -A tailscale up --accept-routes
        ;;
    3)  if tailscale status ; then
            notify-send -r "$notify_id" "👾 Tailscale" "Stopping daemon & client"
            sudo -A tailscale down && sudo -A systemctl stop tailscaled
        else
            notify-send -r "$notify_id" "👾 Tailscale" "Starting daemon & client"
            sudo -A systemctl start tailscaled && sudo -A tailscale up --accept-routes
        fi
        ;;
esac >/dev/null 2>&1

if tailscale status >/dev/null 2>&1 ; then
    echo "👾 $(tailscale ip -4)"
else
    echo "👾 ⏹"
fi
