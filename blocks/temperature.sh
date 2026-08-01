#!/usr/bin/env dash
set -e

case "$(sensors)" in
    *"coretemp"*) echo "🌡 $(sensors coretemp-isa-0000 | sed -n 's/Package id 0:\s\++\([0-9]\{2\}\.[0-9]\).*/\1/p')°C" ;;
    *"thinkpad"*) echo "🌡 $(sensors thinkpad-isa-0000 | sed -n 's/CPU:\s\++\([0-9]\{2\}\.[0-9]\).*/\1/p')°C"
esac
