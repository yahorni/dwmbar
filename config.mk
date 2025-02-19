# dwmbar version
VERSION = 0.0.1

# Customize below to fit your system

# paths
PREFIX = ~/.local
MANPREFIX = ${PREFIX}/share/man
BLOCKSPREFIX = ${PREFIX}/share/dwmbar

PKG_CONFIG = pkg-config

# includes and libs
INCS =
LIBS = -lpthread -lX11

# flags
CPPFLAGS = -D_POSIX_C_SOURCE=200809L -DVERSION=\"${VERSION}\" -DBLOCKSPREFIX=\"${BLOCKSPREFIX}\"
#CFLAGS   = -g -std=c99 -pedantic -Wall -O0 ${INCS} ${CPPFLAGS}
CFLAGS   = -std=c99 -pedantic -Wall -Wextra -Wno-deprecated-declarations -Os ${INCS} ${CPPFLAGS}
LDFLAGS  = ${LIBS}

# compiler and linker
CC = cc
