#!/bin/dash

case $BLOCK_BUTTON in
    1)
        if systemctl is-active tailscaled >/dev/null 2>&1 ; then
            if tailscale status >/dev/null 2>&1 ; then
                status_msg="ipv4 $(tailscale ip -4)\n$(grep nameserver /etc/resolv.conf)"
            else
                status_msg="client: stopped"
            fi
            notify-send "👾 Tailscale" "$status_msg"
        else
            notify-send "👾 Tailscale" "daemon: stopped"
        fi
        ;;
    2) set -x
        notify-send "👾 Tailscale" "Restarting"
        if ! systemctl is-active tailscaled >/dev/null 2>&1 ; then
            sudo -A systemctl start tailscaled >/dev/null 2>&1
        elif tailscale status >/dev/null 2>&1 ; then
            sudo -A tailscale down >/dev/null 2>&1
        fi
        sudo -A tailscale up >/dev/null 2>&1
        ;;
    3)
        if tailscale status >/dev/null 2>&1 ; then
            sudo -A tailscale down >/dev/null 2>&1 &&\
                sudo -A systemctl stop tailscaled >/dev/null 2>&1
        else
            sudo -A systemctl start tailscaled >/dev/null 2>&1 && \
                sudo -A tailscale up >/dev/null 2>&1
        fi
        ;;
esac

if tailscale status >/dev/null 2>&1 ; then 
    echo "👾 $(tailscale ip -4)"
else
    echo "👾 ⏹"
fi
