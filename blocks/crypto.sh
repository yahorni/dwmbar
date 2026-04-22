#!/usr/bin/env dash
set -e

price_btc="$(curl -s "rate.sx/1btc")"
price_eth="$(curl -s "rate.sx/1eth")"
printf "💰 %0.2f 🍷 %0.2f" "$price_btc" "$price_eth"
