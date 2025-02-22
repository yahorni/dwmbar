/* See LICENSE file for copyright and license details. */

/* path to dwmbar fifo */
static const char fifo_path[] = "/tmp/dwmbar.fifo";

/* maximum length of a single block */
#define BLOCK_OUTPUT_LEN 1000

/* delimiter for blocks
 * set to NULL to don't show delimiter
 * (WARN: don't set non-printable character) */
static const char delimiter = '|';

/* should there be spaces around delimiter [0/1]
 * works only when delimiter enabled */
static const int with_spaces = 1;

/* placeholder for block which doesn't output anything */
static const char empty_block[] = "...";

/* block types */
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

/* long running services to listen for their output */
static const Service services[] = {
    { "xkb-switch -w",          1,  Keyboard,   NULL },
    { "acpi_listen",            0,  Volume,     NULL },
    { "playerctl -F status",    0,  Player,     NULL },

    // TODO: implement patterns
    // { "mpc idle",               1,  Player,     {"player"} },
    // { "acpi_listen",            0,  Volume,     {"button/mute", "button/volume"} },
};
