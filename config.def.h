/* blocks path */
#define BLOCKS "~/.local/share/dwmbar/blocks"

/* setup your blocks here */
static const Block blocks[] = {
    /* name */       /* command */      /* interval */
    { "player",      "mpd.sh",          10 },
    { "torrent",     "transmission.sh", 15 },
    { "volume",      "pamixer.sh",      5  },
    { "bright",      "xbacklight.sh",   10 },
    { "network",     "network.sh",      5  },
    { "memory",      "memory.sh",       5  },
    { "cpu",         "cpu-bars.sh",     1  },
    { "battery",     "acpi.sh",         10 },
    { "temperature", "temperature.sh",  10 },
    { "keyboard",    "xkb-switch.sh",   10 },
    { "datetime",    "datetime.sh",     60 },
};

/* drawable delimiter for blocks
 * (WARN: don't set non-printable character) */
static const char delimiter = '|';

/* should there be spaces around delimiter [0/1] */
static const int withSpaces = 1;

/* path to dwmbar fifo */
static const char fifoPath[] = "/tmp/dwmbar.fifo";
