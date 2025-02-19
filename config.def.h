enum BlockType {
    Player,
    VPN,
    Volume,
    Network,
    RAM,
    CPU,
    Temperature,
    Keyboard,
    DateTime,
};

/* setup your blocks here */
static const Block blocks[] = {
                      /* name */        /* command */       /* interval */
    [Player]      = { "player",         "playerctl.sh",     10 },
    [Volume]      = { "volume",         "pamixer.sh",       5  },
    [Network]     = { "network",        "network.sh",       5  },
    [RAM]         = { "ram",            "ram.sh",           5  },
    [CPU]         = { "cpu",            "cpu-bars.sh",      1  },
    [Temperature] = { "temperature",    "temperature.sh",   10 },
    // TODO: set `xkb-switch` by default
    [Keyboard]    = { "keyboard",       "xkb-switch.sh",    10 },
    // TODO: set `date '+%R %d/%m/%Y %a'` by default
    [DateTime]    = { "datetime",       "datetime.sh",      60 },
};

/* drawable delimiter for blocks
 * (WARN: don't set non-printable character) */
static const char delimiter = '|';

/* should there be spaces around delimiter [0/1] */
static const int with_spaces = 1;

/* path to dwmbar fifo */
static const char fifo_path[] = "/tmp/dwmbar.fifo";

/* long running services to listen for their output */
static const Service services[] = {
    // TODO: implement run_oneshot_process
    // { "xkb-switch -w",  1,  Keyboard,   NULL },
    // TODO: implement pattern
    { "acpi_listen",    "player",   NULL },
};
