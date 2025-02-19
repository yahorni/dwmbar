/* See LICENSE file for copyright and license details. */

#define __STDC_WANT_LIB_EXT1__ 1
#include <X11/Xlib.h>
#include <errno.h>
#include <fcntl.h>
#include <pthread.h>
#include <signal.h>
#include <stdarg.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/select.h>
#include <sys/stat.h>
#include <sys/types.h>
#include <unistd.h>
#include <wordexp.h>

#include "util.h"

#define BAR_LEN 510
#define BLOCKS_PATH_LEN 250
#define BLOCK_CMD_LEN 510
#define BLOCK_OUTPUT_LEN 60
#define FIFO_BUFFER_LEN 250
#define EMPTY_CMD_OUTPUT "..."
#define SERVICE_READ_BUFFER_LEN 510

#define LOG_PREFIX "dwmbar: "
#define DEBUG_LOG_PREFIX "dwmbar(debug): "
#define ERROR_LOG_PREFIX "dwmbar(error): "

typedef struct {
    char *name;
    char *command;
    unsigned int interval;
} Block;

typedef struct {
    char *command;
    int oneshot;
    int block;
    char *pattern;
} Service;

/* function declarations */

static void set_status(char *status);
static void update_status();
static void run_command(const Block *block, char *output, int button);
static size_t get_block_index(const char *name);

static void *periodic_updater(void *vargp);
static void *run_service(void *vargp);
static void run_oneshot_process(Service *service);
static void run_continuous_process(Service *service);
static void stop_service(pid_t pid, char *command);

static void signal_handler(int signum);
static void restart_handler(int signum);

static void expand_blocks_path();
static void remove_fifo_if_exists();
static void create_fifo();
static int open_fifo();

static void run();
static void cleanup();

/* variables */
static char old_status[BAR_LEN + 1];
static char new_status[BAR_LEN + 1];
static char current_command[BLOCK_CMD_LEN + 1];
static char blocks_path[BLOCKS_PATH_LEN + 1];

static Display *dpy;
static int screen;
static Window root_window;

volatile sig_atomic_t is_running = 1;
static int is_restart = 0;

pthread_t updater_thread_id;
pthread_mutex_t update_status_lock = PTHREAD_MUTEX_INITIALIZER;
pthread_mutex_t run_command_lock = PTHREAD_MUTEX_INITIALIZER;

/* configuration, allows nested code to access above variables */
#include "config.h"

#define BLOCKS_AMOUNT (sizeof(blocks) / sizeof(blocks[0]))
static char blocks_cache[BLOCKS_AMOUNT][BLOCK_OUTPUT_LEN + 1];

#define SERVICES_AMOUNT (sizeof(services) / sizeof(services[0]))
static pthread_t services_thread_id[SERVICES_AMOUNT];

/* function implementations */

void set_status(char *status) {
    if (!(dpy = XOpenDisplay(NULL))) {
        fprintf(stderr, LOG_PREFIX "cannot open display");
        is_running = 0;
    }
    screen = DefaultScreen(dpy);
    root_window = RootWindow(dpy, screen);
    // fprintf(stderr, DEBUG_LOG_PREFIX "new status:\n%s\n", new_status);
    XStoreName(dpy, root_window, status);
    XCloseDisplay(dpy);
}

void update_status() {
    pthread_mutex_lock(&update_status_lock);

    /* position where to start writing */
    int k = 0;
    int delim_width = with_spaces ? 4 : 2;
    /* leave space for last \0 */
    for (size_t i = 0; i < BLOCKS_AMOUNT && k < BAR_LEN - 1; i++) {
        /* check i > 0 to skip first block */
        if (i && k + delim_width < BAR_LEN - 1) {
            if (with_spaces) new_status[k++] = ' ';
            new_status[k++] = delimiter;
            new_status[k++] = '\n';
            if (with_spaces) new_status[k++] = ' ';
        }

        append_buffer(new_status + k, blocks_cache[i], BAR_LEN - k - 1);
        k = strlen(new_status);
    }

    /* if new status equals to old then do not update*/
    if (!strcmp(new_status, old_status)) {
        /* clear new status */
        reset_buffer(new_status, BAR_LEN);
        pthread_mutex_unlock(&update_status_lock);
        return;
    }

    /* clear and reset old status */
    reset_buffer(old_status, BAR_LEN);
    copy_buffer(old_status, new_status, BAR_LEN);

    /* update status */
    set_status(new_status);

    /* clear new status */
    reset_buffer(new_status, BAR_LEN);

    pthread_mutex_unlock(&update_status_lock);
}

/* Opens process *current_command and stores output in *output */
void run_command(const Block *block, char *output, int button) {
    pthread_mutex_lock(&run_command_lock);

    reset_buffer(output, BLOCK_OUTPUT_LEN);
    reset_buffer(current_command, BLOCK_CMD_LEN);

    copy_buffer(output, EMPTY_CMD_OUTPUT, sizeof(EMPTY_CMD_OUTPUT));

    FILE *command_file;
    if (button) {
        sprintf(current_command, "BLOCK_BUTTON=%d %s/%s", button, blocks_path, block->command);
        button = 0;
    } else {
        sprintf(current_command, "%s/%s", blocks_path, block->command);
    }
    // fprintf(stderr, DEBUG_LOG_PREFIX "executing: '%s'\n", current_command);

    if (!(command_file = popen(current_command, "r"))) {
        fprintf(stderr, LOG_PREFIX "Failed to execute: '%s'\n", current_command);
        return;
    }

    if (fgets(output, BLOCK_OUTPUT_LEN, command_file)) {
        int new_size = remove_from_buffer(output, '\n');
        output[new_size] = '\0';
    } else {
        fprintf(stderr, LOG_PREFIX "Failed to read command output: '%s'\n", current_command);
        // FIXME: not sure what to do here
    }

    pclose(command_file);

    pthread_mutex_unlock(&run_command_lock);
}

size_t get_block_index(const char *name) {
    for (size_t i = 0; i < BLOCKS_AMOUNT; i++) {
        if (strcmp(blocks[i].name, name) == 0) return i;
    }

    return -1;
}

void *periodic_updater(void *vargp) {
    /* suppress warn */
    (void)vargp;

    const Block *current_block;
    unsigned int time = 0;

    while (is_running) {
        for (size_t i = 0; i < BLOCKS_AMOUNT; i++) {
            current_block = blocks + i;
            if (time % current_block->interval == 0) {
                run_command(current_block, blocks_cache[i], 0);
            }
        }
        update_status();
        sleep(1);
        time++;
    }

    return NULL;
}

void *run_service(void *vargp) {
    Service *service = vargp;
    fprintf(stderr, LOG_PREFIX "Starting service '%s' for block '%s'\n", service->command, blocks[service->block].name);

    if (service->oneshot) {
        run_oneshot_process(service);
    } else {
        run_continuous_process(service);
    }

    return NULL;
}

void run_oneshot_process(Service *service) {
    int block = service->block;
    char read_buffer[SERVICE_READ_BUFFER_LEN + 1];
    int pout;
    pid_t pid;
    FILE *process_output;

    // TODO: wait for oneshot process to finish first, then parse output and return code
    // use standard popen()?
    while (is_running) {
        if ((pid = process_open(service->command, &pout)) == -1) {
            fprintf(stderr, ERROR_LOG_PREFIX "Failed to run service: '%s'\n", service->command);
            return;
        }

        if (!(process_output = fdopen(pout, "r"))) {
            fprintf(stderr, ERROR_LOG_PREFIX "Failed to open service output: '%s'\n", service->command);
            stop_service(pid, service->command);
            return;
        }

        fgets(read_buffer, SERVICE_READ_BUFFER_LEN, process_output);
        fprintf(stderr, DEBUG_LOG_PREFIX "Service read: '%s'\n", service->command);

        run_command(&blocks[block], blocks_cache[block], 0);
        update_status();
    }
}

void run_continuous_process(Service *service) {
    int block = service->block;
    char read_buffer[SERVICE_READ_BUFFER_LEN + 1];

    int pout;
    pid_t pid;
    FILE *process_output;

    if ((pid = process_open(service->command, &pout)) == -1) {
        fprintf(stderr, ERROR_LOG_PREFIX "Failed to run service: '%s'\n", service->command);
        return;
    }

    if (!(process_output = fdopen(pout, "r"))) {
        fprintf(stderr, ERROR_LOG_PREFIX "Failed to open service output: '%s'\n", service->command);
        stop_service(pid, service->command);
        return;
    }

    while (is_running && fgets(read_buffer, SERVICE_READ_BUFFER_LEN, process_output)) {
        fprintf(stderr, DEBUG_LOG_PREFIX "Service read: '%s'\n", service->command);
        run_command(&blocks[block], blocks_cache[block], 0);
        update_status();
    }

    stop_service(pid, service->command);
}

void stop_service(pid_t pid, char *command) {
    if (kill(pid, 0) == ESRCH) {
        fprintf(stderr, ERROR_LOG_PREFIX "Service '%s' already terminated\n", command);
        return;
    }

    if (kill(pid, SIGTERM)) {
        fprintf(stderr, ERROR_LOG_PREFIX "Failed to terminate service: '%s'\n", command);
    }
}

void signal_handler(int signum) {
    // TODO: probably need to get rid of fprintf
    // https://stackoverflow.com/questions/16891019/how-to-avoid-using-printf-in-a-signal-handler
    fprintf(stderr, LOG_PREFIX "Signal %d: %s. Stopping...\n", signum, strsignal(signum));
    is_running = 0;
}

void restart_handler(int signum) {
    is_restart = 1;
    signal_handler(signum);
}

void expand_blocks_path() {
    wordexp_t exp_result;
    wordexp(BLOCKSPREFIX, &exp_result, 0);
    copy_buffer(blocks_path, exp_result.we_wordv[0], BLOCKS_PATH_LEN);
}

void remove_fifo_if_exists() {
    if (unlink(fifo_path) == 0) {
        fprintf(stderr, LOG_PREFIX "Removed FIFO\n");
    } else if (errno != ENOENT) {
        perror(LOG_PREFIX "unlink() failed");
        return;
    }
}

void create_fifo() {
    /* create fifo */
    if (mkfifo(fifo_path, 0622) < 0) {
        perror(LOG_PREFIX "mkfifo() failed");
        return;
    }
}

int open_fifo() {
    // https://stackoverflow.com/questions/21468856/check-if-file-is-a-named-pipe-fifo-in-c#comment32401662_21468960

    int fifo_fd = -1;

    /* open fifo in non-block mode to check type */
    if ((fifo_fd = open(fifo_path, O_RDWR | O_NONBLOCK)) < 0) {
        perror(LOG_PREFIX "open() failed");
        return -1;
    }

    /* read fifo stats */
    struct stat fifo_stat;
    if (fstat(fifo_fd, &fifo_stat) < 0) {
        perror(LOG_PREFIX "fstat() failed");
        return -1;
    }

    /* check that file is fifo */
    if (!S_ISFIFO(fifo_stat.st_mode)) {
        fprintf(stderr, LOG_PREFIX "Given file is not FIFO\n");
        return -1;
    }

    return fifo_fd;
}

sigset_t setup_signals() {
    /* install signal handler for SIGINT, SIGHUP */
    struct sigaction sa;
    sa.sa_handler = signal_handler;
    sa.sa_flags = 0;
    sigemptyset(&sa.sa_mask);
    sigaction(SIGINT, &sa, NULL);
    sigaction(SIGHUP, &sa, NULL);

    /* install SIGUSR1 for restarting */
    struct sigaction rsa;
    rsa.sa_handler = restart_handler;
    rsa.sa_flags = 0;
    sigemptyset(&rsa.sa_mask);
    sigaction(SIGUSR1, &rsa, NULL);

    /* block signals */
    sigset_t sigset, oldset;
    sigemptyset(&sigset);
    sigaddset(&sigset, SIGINT);
    sigaddset(&sigset, SIGHUP);
    sigaddset(&sigset, SIGUSR1);
    sigprocmask(SIG_BLOCK, &sigset, &oldset);

    return oldset;
}

void run() {
    fprintf(stderr, LOG_PREFIX "Starting bar...\n");
    fprintf(stderr, LOG_PREFIX "FIFO file: %s\n", fifo_path);

    /* expand full blocks path in case it has '~' or any env variables */
    expand_blocks_path();

    /* set periodical updates */
    if (pthread_create(&updater_thread_id, NULL, &periodic_updater, NULL)) {
        die(LOG_PREFIX "pthread_create() failed");
    }

    /* start services */
    for (size_t i = 0; i < SERVICES_AMOUNT; i++) {
        // fprintf(stderr, DEBUG_LOG_PREFIX "Pre-start service: %s, block: %d\n", services[i].command, services[i].block);

        if (pthread_create(&services_thread_id[i], NULL, &run_service, (void *)&services[i])) {
            die(LOG_PREFIX "pthread_create() failed for service: %s", blocks[services[i].block].name);
        }
    }

    /* setup signals, store old set for pselect */
    sigset_t oldset = setup_signals();

    char fifo_buffer[FIFO_BUFFER_LEN + 1] = {0};
    char block_name_buffer[FIFO_BUFFER_LEN + 1] = {0};

    fd_set read_fds;
    fd_set error_fds;

    remove_fifo_if_exists();
    create_fifo();

    while (is_running) {
        fprintf(stderr, LOG_PREFIX "Opening FIFO...\n");
        int fifo_fd = -1;
        if ((fifo_fd = open_fifo()) < 0) {
            fprintf(stderr, LOG_PREFIX "Failed to create FIFO");
            break;
        }

        fprintf(stderr, LOG_PREFIX "Reading FIFO...\n");
        while (is_running) {
            unsigned int button = 0;
            reset_buffer(fifo_buffer, FIFO_BUFFER_LEN);
            reset_buffer(block_name_buffer, FIFO_BUFFER_LEN);

            FD_ZERO(&read_fds);
            FD_ZERO(&error_fds);
            FD_SET(fifo_fd, &read_fds);

            if (pselect(fifo_fd + 1, &read_fds, NULL, &error_fds, NULL, &oldset) < 0) {
                perror(LOG_PREFIX "pselect() caught signal");
                is_running = 0;
                break;
            }

            if (FD_ISSET(fifo_fd, &error_fds)) {
                fprintf(stderr, LOG_PREFIX "fifo fd error\n");
                continue;
            }

            if (!FD_ISSET(fifo_fd, &read_fds)) {
                fprintf(stderr, LOG_PREFIX "fifo fd not in read fdset\n");
                continue;
            }

            int nread = read(fifo_fd, fifo_buffer, FIFO_BUFFER_LEN);
            if (nread < 0) {
                perror(LOG_PREFIX "read() failed");
                continue;
            } else if (nread == 0) {
                if (errno == EAGAIN) {
                    fprintf(stderr, LOG_PREFIX "read() EAGAIN\n");
                    continue;
                } else {
                    fprintf(stderr, LOG_PREFIX "read() received EOF\n");
                    // perror(LOG_PREFIX "read() received EOF");
                    break;
                }
            }

            if (sscanf(fifo_buffer, "%s %u\n", block_name_buffer, &button) < 0) {
                perror(LOG_PREFIX "sscanf() failed");
                continue;
            }

            int block = 0;
            if (is_number(block_name_buffer, FIFO_BUFFER_LEN)) {
                /* used for direct clicks on bar */
                block = atoi(block_name_buffer);
                /* input block starts with 1, not 0, so we decrease it */
                block--;
            } else {
                /* used for updates coming from pipe */
                block = get_block_index(block_name_buffer);
                if (block == -1) {
                    fprintf(stderr, LOG_PREFIX "Invalid block name: %s\n", block_name_buffer);
                    continue;
                }
            }

            run_command(&blocks[block], blocks_cache[block], button);
            update_status();
        }

        fprintf(stderr, LOG_PREFIX "Closing FIFO...\n");
        if (close(fifo_fd) < 0) {
            perror(LOG_PREFIX "close() failed");
            break;
        }
    }
}

void cleanup() {
    fprintf(stderr, LOG_PREFIX "Cleaning up...\n");

    remove_fifo_if_exists();

    pthread_mutex_destroy(&run_command_lock);
    pthread_mutex_destroy(&update_status_lock);
    pthread_join(updater_thread_id, NULL);

    for (size_t i = 0; i < SERVICES_AMOUNT; i++) {
        pthread_join(services_thread_id[i], NULL);
    }
}

int main(int argc, char **argv) {
    if (argc == 2 && !strcmp("-v", argv[1]))
        die("dwmbar-" VERSION);
    else if (argc != 1)
        die("usage: dwm [-v]");

    run();
    cleanup();

    if (is_restart) {
        sigset_t sigs;
        sigprocmask(SIG_SETMASK, &sigs, 0);
        execvp(argv[0], argv);
    }

    return EXIT_SUCCESS;
}
