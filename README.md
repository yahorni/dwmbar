# dwmbar

Simple dwm status bar in C

Inspired by:
- [dwm](https://dwm.suckless.org)
- [Luke Smith's dwmblocks](https://github.com/LukeSmithxyz/dwmblocks)
- [i3blocks](https://github.com/vivien/i3blocks)

```bash
# run
dwmbar

# build and install
make install
# uninstall
make uninstall
# clean artifacts
make clean

# run beforehand to install/uninstall in project directory
export DESTDIR=$PWD

# restart
pkill -USR1 dwmbar
```
