#!/bin/dash

case $BLOCK_BUTTON in
    1)
        if systemctl is-active tailscaled ; then
            if tailscale status ; then
                status_msg="ipv4 $(tailscale ip -4)\n$(grep nameserver /etc/resolv.conf)"
            else
                status_msg="client: stopped"
            fi
            notify-send "👾 Tailscale" "$status_msg"
        else
            notify-send "👾 Tailscale" "daemon: stopped"
        fi
        ;;
    2)
        notify-send "👾 Tailscale" "Restarting"
        if ! systemctl is-active tailscaled ; then
            sudo -A systemctl start tailscaled
        elif tailscale status ; then
            sudo -A tailscale down
        fi
        sudo -A tailscale up --accept-routes
        ;;
    3)
        notify-send "👾 Tailscale" "Toggling"
        if tailscale status ; then
            sudo -A tailscale down &&\
                sudo -A systemctl stop tailscaled
        else
            sudo -A systemctl start tailscaled && \
                sudo -A tailscale up --accept-routes
        fi
        ;;
esac >/dev/null 2>&1

if tailscale status >/dev/null 2>&1 ; then
    echo "👾 $(tailscale ip -4)"
else
    echo "👾 ⏹"
fi
