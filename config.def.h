/* setup your blocks here */
static const Block blocks[] = {
      /* name */        /* command */           /* interval */
    { "player",         "playerctl.sh",         10 },
    { "volume",         "pamixer.sh",           5  },
    { "network",        "network.sh",           5  },
    { "memory",         "memory.sh",            5  },
    { "cpu",            "cpu-bars.sh",          1  },
    { "temperature",    "temperature.sh",       10 },
    { "keyboard",       "xkb-switch.sh",        10 },
    { "datetime",       "datetime.sh",          60 },
};

/* drawable delimiter for blocks
 * (WARN: don't set non-printable character) */
static const char delimiter = '|';

/* should there be spaces around delimiter [0/1] */
static const int with_spaces = 1;

/* path to dwmbar fifo */
static const char fifo_path[] = "/tmp/dwmbar.fifo";
