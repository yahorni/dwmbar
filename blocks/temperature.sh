#!/usr/bin/env dash
set -e

# run `sensors` and set desired sensor
# example with "coretemp-isa-0000" sensor and "Package id 0" temperature:
# echo "🌡 $(sensors coretemp-isa-0000 | sed -n 's/Package id 0:\s\++\([0-9]\{2\}\.[0-9]\).*/\1/p')°C"

echo "🌡 $(sensors thinkpad-isa-0000 | sed -n 's/CPU:\s\++\([0-9]\{2\}\.[0-9]\).*/\1/p')°C"
