/* See LICENSE file for copyright and license details. */

/* path to dwmbar fifo */
static const char fifo_path[] = "/tmp/dwmbar.fifo";

/* maximum length of a single block */
#define BLOCK_OUTPUT_LEN 100

/* delimiter for blocks
 * set to NULL to don't show delimiter
 * (WARN: don't set non-printable character) */
static const char delimiter = '|';

/* should there be spaces around delimiter [0/1]
 * works only when delimiter enabled */
static const int with_spaces = 1;

/* placeholder for block which doesn't output anything */
static const char empty_block[] = "...";

/* setup your blocks here */
// TODO: implement using commands directly (without script), e.g. `xkb-switch` or `date '+%R %d/%m/%Y %a'`
static const Block blocks[] = {
    /* name */      /* command */       /* interval */
    { "player",     "player.sh",        5   },
    { "volume",     "pamixer.sh",       5   },
    { "network",    "network.sh",       5   },
    { "ram",        "ram.sh",           5   },
    { "cpu",        "cpu-bars.sh",      1   },
    { "temperature","temperature.sh",   10  },
    { "keyboard",   "xkb-switch.sh",    10  },
    { "datetime",   "datetime.sh",      60  },
};

/* long running services to listen for their output */
/* define USE_SERVICES to compile with services */
#define USE_SERVICES
static const Service services[] = {
    /* block */     /* command */           /* filter */    /* oneshot */
    { "keyboard",   "xkb-switch -w",        NULL,           1 },
    // { "keyboard",   "xkb-switch -W",        NULL,           0 },
    { "volume",     "acpi_listen",          "button/",      0 },
    // { "volume",     "pactl subscribe",      " on client #", 0 },
    { "player",     "playerctl -F status",  NULL,           0 },
    { "player",     "playerctl -F metadata -f '{{ xesam:title }}'", NULL, 0},
    // { "player",     "mpc idle",             "player",       1 },
};
