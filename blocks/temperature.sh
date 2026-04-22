#!/usr/bin/env dash
set -e

echo "🌡 $(sensors coretemp-isa-0000 | sed -n 's/Package id 0:.\++\([0-9]\+\+\).*(.*$/\1/p')°C"
