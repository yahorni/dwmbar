#!/bin/dash

case $BLOCK_BUTTON in
    3) setsid "$TERMINAL" -c dropdown -e bash -c "cal -3 --monday ; read -rsp $'Press any key to exit\n' -n1" & ;;
esac

echo "📅 $(date '+%R %d/%m/%Y %a')"
