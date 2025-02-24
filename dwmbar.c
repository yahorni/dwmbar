/* See LICENSE file for copyright and license details. */

#define __STDC_WANT_LIB_EXT1__ 1
#include <X11/Xlib.h>
#include <errno.h>
#include <fcntl.h>
#include <pthread.h>
#include <signal.h>
#include <stdarg.h>
#include <stdbool.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/select.h>
#include <sys/stat.h>
#include <sys/types.h>
#include <sys/wait.h>
#include <unistd.h>
#include <wordexp.h>

#include "util.h"

typedef struct {
    char *name;
    char *command;
    unsigned int interval;
} Block;

typedef struct {
    char *command;
    bool oneshot;
    int block_index;
    char *pattern;
} Service;

/* configuration */

#include "config.h"

/* definitions */

#define BAR_LEN 1000
/* compile time check for block length */
struct BlockLenCheck {
    char block_longer_than_bar[BLOCK_OUTPUT_LEN > BAR_LEN ? -1 : 1];
};

#define BLOCKS_AMOUNT (sizeof(blocks) / sizeof(blocks[0]))
#define SERVICES_AMOUNT (sizeof(services) / sizeof(services[0]))
#define EMPTY_BLOCK_SIZE (sizeof(empty_block) / sizeof(empty_block[0]))

#define BLOCKS_PATH_LEN 250
#define BLOCK_CMD_LEN 510
#define FIFO_BUFFER_LEN 250
#define SERVICE_READ_BUFFER_LEN 510

#define ERROR_LOG_PREFIX "dwmbar(error): "
#define WARN_LOG_PREFIX "dwmbar(warn ): "
#define INFO_LOG_PREFIX "dwmbar(info ): "
#define DEBUG_LOG_PREFIX "dwmbar(debug): "

#define SERVICE_PREFIX "service '%s': "
#define UPDATER_PREFIX "updater: "

/* internal types */

typedef struct {
    Service *service;
    int pipe_fd;
} ServiceThreadArgs;

typedef struct {
    char *command;
    int block_index;
    int pipe_fd;
    char *read_buffer;
    pid_t pid;
    int process_fd;
} ServiceContext;

/* variables */

/* status bar */
static char old_status[BAR_LEN + 1];
static char new_status[BAR_LEN + 1];
static pthread_mutex_t update_status_lock = PTHREAD_MUTEX_INITIALIZER;

/* blocks */
static char current_command[BLOCK_CMD_LEN + 1];
static char blocks_path[BLOCKS_PATH_LEN + 1];
static char blocks_cache[BLOCKS_AMOUNT][BLOCK_OUTPUT_LEN + 1];
static pthread_mutex_t run_command_lock = PTHREAD_MUTEX_INITIALIZER;

/* X11 */
static Display *dpy;
static int screen;
static Window root_window;

/* periodic_updater */
static pthread_t updater_thread_id;
static int updater_pipe[2];

/* services */
static pthread_t services_thread_ids[SERVICES_AMOUNT];
static int services_pipes[SERVICES_AMOUNT][2];
static ServiceThreadArgs services_args[SERVICES_AMOUNT];

/* signals */
static sigset_t default_sigset;
static int is_restart = 0;

/* execution flow */
volatile sig_atomic_t is_running = 1;

/* function declarations */

/* status bar */
static void update_status_buffer();
static void set_x11_status(char *status);

/* blocks */
static void expand_blocks_path();
static void run_block_command(int block_index, int button);
static int get_block_index(const char *name);

/* periodic updater */
static void *periodic_updater(void *vargp);
static bool start_updater();
static void cleanup_updater();

/* services */
static void *run_service(void *vargp);
static void run_oneshot_service(ServiceContext *ctx);
static void run_continuous_service(ServiceContext *ctx);
static bool read_from_service(ServiceContext *ctx);
static void stop_service(ServiceContext *ctx);
static bool start_services();
static void cleanup_service(int index);
static void cleanup_services();

/* signals */
static void signal_handler(int signum);
static void restart_handler(int signum);
static bool setup_signals();

/* fifo */
static int create_fifo();
static int open_fifo();
static int remove_fifo_if_exists();
static int read_from_fifo(int fifo_fd, char *fifo_buffer, sigset_t *sigset);

/* commands */
static int parse_command(char *in, char *out, int *button);
static void handle_commands_from_fifo(char *fifo_buffer, char *block_name_buffer);

/* execution flow */
static bool setup();
static void run();
static void cleanup();

/* function implementations */

/* status bar */

void update_status_buffer() {
    pthread_mutex_lock(&update_status_lock);

    /* position where to start writing */
    int k = 0;
    int delimiter_width = with_spaces ? 4 : 2;
    /* leave space for last \0 */
    for (size_t i = 0; i < BLOCKS_AMOUNT && k < BAR_LEN - 1; i++) {
        /* check i > 0 to skip first block */
        if (i && k + delimiter_width < BAR_LEN - 1) {
            if (with_spaces) new_status[k++] = ' ';
            if (delimiter) new_status[k++] = delimiter;
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
    set_x11_status(new_status);

    /* clear new status */
    reset_buffer(new_status, BAR_LEN);

    pthread_mutex_unlock(&update_status_lock);
}

void set_x11_status(char *status) {
    if (!(dpy = XOpenDisplay(NULL))) {
        fprintf(stderr, ERROR_LOG_PREFIX "cannot open display");
        is_running = 0;
    }
    screen = DefaultScreen(dpy);
    root_window = RootWindow(dpy, screen);
    // fprintf(stderr, DEBUG_LOG_PREFIX "new status:\n%s\n", new_status);
    XStoreName(dpy, root_window, status);
    XCloseDisplay(dpy);
}

/* blocks */

/* Expands env variables and '~' in blocks path */
void expand_blocks_path() {
    wordexp_t expanded;
    wordexp(BLOCKSPREFIX, &expanded, 0);
    copy_buffer(blocks_path, expanded.we_wordv[0], BLOCKS_PATH_LEN);
    wordfree(&expanded);
}

/* Opens process *current_command and stores output in *output */
void run_block_command(int block_index, int button) {
    pthread_mutex_lock(&run_command_lock);

    const Block *block = &blocks[block_index];
    char *output = blocks_cache[block_index];

    reset_buffer(output, BLOCK_OUTPUT_LEN);
    reset_buffer(current_command, BLOCK_CMD_LEN);

    copy_buffer(output, empty_block, EMPTY_BLOCK_SIZE);

    FILE *command_file;
    if (button) {
        sprintf(current_command, "BLOCK_BUTTON=%d %s/%s", button, blocks_path, block->command);
    } else {
        sprintf(current_command, "%s/%s", blocks_path, block->command);
    }

    // fprintf(stderr, DEBUG_LOG_PREFIX "executing: '%s'\n", current_command);
    if (!(command_file = popen(current_command, "r"))) {
        fprintf(stderr, ERROR_LOG_PREFIX "failed to execute: '%s'\n", current_command);
        pthread_mutex_unlock(&run_command_lock);
        return;
    }

    // FIXME?: blocking call
    if (fgets(output, BLOCK_OUTPUT_LEN, command_file)) {
        int new_size = remove_from_buffer(output, '\n');
        output[new_size] = '\0';
    } else {
        fprintf(stderr, WARN_LOG_PREFIX "failed to read command output: '%s'\n", current_command);
        // FIXME: not sure what to do here
    }

    pclose(command_file);

    pthread_mutex_unlock(&run_command_lock);
}

int get_block_index(const char *name) {
    for (size_t i = 0; i < BLOCKS_AMOUNT; i++) {
        if (strcmp(blocks[i].name, name) == 0) return i;
    }

    return -1;
}

/* periodic updater */

void *periodic_updater(void *vargp) {
    /* suppress warn */
    (void)vargp;

    const Block *current_block;
    unsigned int time = 0;

    while (is_running) {
        for (size_t i = 0; i < BLOCKS_AMOUNT; i++) {
            current_block = blocks + i;
            if (time % current_block->interval == 0) {
                run_block_command(i, 0);
            }
        }
        update_status_buffer();

        fd_set read_fds;
        FD_ZERO(&read_fds);
        FD_SET(updater_pipe[0], &read_fds);
        struct timeval tv = {1, 0};

        int retval = select(updater_pipe[0] + 1, &read_fds, NULL, NULL, &tv);
        if (retval < 0) {
            perror(ERROR_LOG_PREFIX UPDATER_PREFIX "select() failed");
            return NULL;
        } else if (retval) {
            fprintf(stderr, INFO_LOG_PREFIX UPDATER_PREFIX "received message to stop\n");
            return NULL;
        }

        time++;
    }

    return NULL;
}

bool start_updater() {
    if (pipe(updater_pipe) < 0) {
        fprintf(stderr, ERROR_LOG_PREFIX "pipe() failed for updater");
        perror(NULL);
        return false;
    }

    if (pthread_create(&updater_thread_id, NULL, &periodic_updater, NULL)) {
        perror(INFO_LOG_PREFIX "pthread_create() failed for periodic updater");
        return false;
    }

    return true;
}

void cleanup_updater() {
    write(updater_pipe[1], "", 1);
    fprintf(stderr, DEBUG_LOG_PREFIX "waiting for updater\n");
    pthread_join(updater_thread_id, NULL);
}

/* services */

void *run_service(void *vargp) {
    ServiceThreadArgs *args = vargp;
    char read_buffer[SERVICE_READ_BUFFER_LEN + 1];
    ServiceContext ctx = {
        .command = args->service->command,
        .block_index = args->service->block_index,
        .pipe_fd = args->pipe_fd,
        .read_buffer = read_buffer,
    };

    fprintf(stderr, INFO_LOG_PREFIX SERVICE_PREFIX "starting for block '%s'\n", ctx.command,
            blocks[ctx.block_index].name);

    if (args->service->oneshot)
        run_oneshot_service(&ctx);
    else
        run_continuous_service(&ctx);

    return NULL;
}

void run_oneshot_service(ServiceContext *ctx) {
    while (is_running) {
        if ((ctx->pid = process_open(ctx->command, &ctx->process_fd)) == -1) {
            fprintf(stderr, ERROR_LOG_PREFIX SERVICE_PREFIX "failed to run: ", ctx->command);
            perror(NULL);
            return;
        }

        fprintf(stderr, DEBUG_LOG_PREFIX SERVICE_PREFIX "pid: %d\n", ctx->command, ctx->pid);

        if (read_from_service(ctx)) {
            run_block_command(ctx->block_index, 0);
            update_status_buffer();
        }

        stop_service(ctx);
    }
}

void run_continuous_service(ServiceContext *ctx) {
    if ((ctx->pid = process_open(ctx->command, &ctx->process_fd)) == -1) {
        fprintf(stderr, ERROR_LOG_PREFIX SERVICE_PREFIX "failed to run: ", ctx->command);
        perror(NULL);
        return;
    }

    fprintf(stderr, DEBUG_LOG_PREFIX SERVICE_PREFIX "pid: %d\n", ctx->command, ctx->pid);

    while (is_running) {
        if (read_from_service(ctx)) {
            run_block_command(ctx->block_index, 0);
            update_status_buffer();
        } else {
            stop_service(ctx);
            return;
        }
    }
}

bool read_from_service(ServiceContext *ctx) {
    /* no need to check max(process_fd, pipe_fd) as pipe_fd created earlier */
    int nfds = ctx->process_fd + 1;
    fd_set read_fds;

    FD_ZERO(&read_fds);
    FD_SET(ctx->process_fd, &read_fds);
    FD_SET(ctx->pipe_fd, &read_fds);

    if (select(nfds, &read_fds, NULL, NULL, NULL) < 0) {
        fprintf(stderr, ERROR_LOG_PREFIX SERVICE_PREFIX "select() error: ", ctx->command);
        perror(NULL);
        return false;
    }

    if (FD_ISSET(ctx->process_fd, &read_fds)) {
        fprintf(stderr, DEBUG_LOG_PREFIX SERVICE_PREFIX "ready to read\n", ctx->command);

        int nread = read(ctx->process_fd, ctx->read_buffer, SERVICE_READ_BUFFER_LEN);
        /* TODO: handle multiple read failed to not run infinitely */
        if (nread < 0) {
            fprintf(stderr, ERROR_LOG_PREFIX SERVICE_PREFIX "read() failed", ctx->command);
            perror(NULL);
            return false;
        }

        if (nread > 0) {
            ctx->read_buffer[nread - 1] = '\0';
            fprintf(stderr, DEBUG_LOG_PREFIX SERVICE_PREFIX "message: %s\n", ctx->command, ctx->read_buffer);
        } else {
            fprintf(stderr, DEBUG_LOG_PREFIX SERVICE_PREFIX "read() received EOF\n", ctx->command);
        }
        return true;

    } else if (FD_ISSET(ctx->pipe_fd, &read_fds)) {
        fprintf(stderr, INFO_LOG_PREFIX SERVICE_PREFIX "received message to stop\n", ctx->command);
        return false;
    }

    /* unreachable? */
    fprintf(stderr, ERROR_LOG_PREFIX SERVICE_PREFIX "unreachable reading condition\n", ctx->command);
    return false;
}

void stop_service(ServiceContext *ctx) {
    /* close process output pipe */
    fprintf(stderr, DEBUG_LOG_PREFIX SERVICE_PREFIX "closing output pipe\n", ctx->command);
    if (close(ctx->process_fd)) {
        fprintf(stderr, WARN_LOG_PREFIX SERVICE_PREFIX "error while closing fd pipe: ", ctx->command);
        perror(NULL);
    }

    /* finish process */
    fprintf(stderr, DEBUG_LOG_PREFIX SERVICE_PREFIX "sent signal to terminate\n", ctx->command);
    if (kill(ctx->pid, SIGTERM) != 0) {
        fprintf(stderr, ERROR_LOG_PREFIX SERVICE_PREFIX "failed to terminate with SIGTERM", ctx->command);
        perror(NULL);
    }

    /* wait for process */
    fprintf(stderr, DEBUG_LOG_PREFIX SERVICE_PREFIX "waiting for process\n", ctx->command);
    int wait_status;
    if (waitpid(ctx->pid, &wait_status, 0) < 0) {
        fprintf(stderr, ERROR_LOG_PREFIX SERVICE_PREFIX "failed to wait for pid %d: ", ctx->command, ctx->pid);
        perror(NULL);
    }

    fprintf(stderr, DEBUG_LOG_PREFIX SERVICE_PREFIX "process finished\n", ctx->command);
}

bool start_services() {
    int started = 0;
    for (size_t i = 0; i < SERVICES_AMOUNT; i++) {
        if (pipe(services_pipes[i]) < 0) {
            fprintf(stderr, ERROR_LOG_PREFIX "failed to set pipe for threads communication");
            perror(NULL);
            return false;
        }

        services_args[i].service = (Service *)&services[i];
        services_args[i].pipe_fd = services_pipes[i][0];

        if (pthread_create(&services_thread_ids[i], NULL, &run_service, (void *)&services_args[i])) {
            fprintf(stderr, ERROR_LOG_PREFIX "pthread_create() failed for service '%s'", services[i].command);
        }

        started++;
    }

    if (started != SERVICES_AMOUNT) {
        for (int i = 0; i < started; i++)
            cleanup_service(i);
        return false;
    }

    return true;
}

void cleanup_service(int index) {
    const char *command = services[index].command;
    write(services_pipes[index][1], "", 1);

    fprintf(stderr, DEBUG_LOG_PREFIX "waiting for service '%s'\n", command);
    pthread_join(services_thread_ids[index], NULL);

    close(services_pipes[index][0]);
    close(services_pipes[index][1]);

    fprintf(stderr, DEBUG_LOG_PREFIX "finished service '%s'\n", command);
}

void cleanup_services() {
    for (size_t i = 0; i < SERVICES_AMOUNT; i++) {
        const char *command = services[i].command;
        write(services_pipes[i][1], "", 1);
        fprintf(stderr, DEBUG_LOG_PREFIX "waiting for service '%s'\n", command);
        pthread_join(services_thread_ids[i], NULL);
        fprintf(stderr, DEBUG_LOG_PREFIX "finished service '%s'\n", command);

        close(services_pipes[i][0]);
        close(services_pipes[i][1]);
    }
}

/* signals */

void signal_handler(int signum) {
    /* TODO: probably need to get rid of fprintf */
    /* https://stackoverflow.com/questions/16891019/how-to-avoid-using-printf-in-a-signal-handler */
    fprintf(stderr, INFO_LOG_PREFIX "signal %d: '%s', stopping...\n", signum, strsignal(signum));
    is_running = 0;
}

void restart_handler(int signum) {
    is_restart = 1;
    signal_handler(signum);
}

bool setup_signals() {
    /* install signal handlers */
    struct sigaction sa_exit;
    sa_exit.sa_handler = signal_handler;
    sa_exit.sa_flags = 0;
    sigemptyset(&sa_exit.sa_mask);
    /* dmw is finishing */
    sigaction(SIGHUP, &sa_exit, NULL);
    /* kill -9 */
    sigaction(SIGINT, &sa_exit, NULL);
    /* kill */
    sigaction(SIGTERM, &sa_exit, NULL);

    /* install SIGUSR1 for restarting */
    struct sigaction sa_restart;
    sa_restart.sa_handler = restart_handler;
    sa_restart.sa_flags = 0;
    sigemptyset(&sa_restart.sa_mask);
    /* kill -USR1 */
    sigaction(SIGUSR1, &sa_restart, NULL);

    /* block signals */
    sigset_t sigset;
    sigemptyset(&sigset);
    sigaddset(&sigset, SIGHUP);
    sigaddset(&sigset, SIGINT);
    sigaddset(&sigset, SIGTERM);
    sigaddset(&sigset, SIGUSR1);

    /* sigprocmask for multithreaded app */
    if (pthread_sigmask(SIG_BLOCK, &sigset, &default_sigset)) {
        perror("pthread_sigmask() failed");
        return false;
    }

    return true;
}

/* fifo */

int create_fifo() {
    /* create fifo */
    if (mkfifo(fifo_path, 0622) < 0) {
        perror(WARN_LOG_PREFIX "mkfifo() failed");
        return false;
    }
    return true;
}

int open_fifo() {
    // https://stackoverflow.com/questions/21468856/check-if-file-is-a-named-pipe-fifo-in-c#comment32401662_21468960

    int fifo_fd = -1;

    /* open fifo in non-block mode to check type */
    if ((fifo_fd = open(fifo_path, O_RDWR | O_NONBLOCK)) < 0) {
        perror(ERROR_LOG_PREFIX "open() failed");
        return -1;
    }

    /* read fifo stats */
    struct stat fifo_stat;
    if (fstat(fifo_fd, &fifo_stat) < 0) {
        perror(ERROR_LOG_PREFIX "fstat() for FIFO failed");
        return -1;
    }

    /* check that file is fifo */
    if (!S_ISFIFO(fifo_stat.st_mode)) {
        fprintf(stderr, ERROR_LOG_PREFIX "given file is not FIFO\n");
        return -1;
    }

    return fifo_fd;
}

int remove_fifo_if_exists() {
    if (unlink(fifo_path) == 0) {
        fprintf(stderr, INFO_LOG_PREFIX "removed FIFO\n");
        return true;
    } else if (errno != ENOENT) {
        perror(WARN_LOG_PREFIX "unlink() failed");
        return false;
    }
    return false;
}

int read_from_fifo(int fifo_fd, char *fifo_buffer, sigset_t *sigset) {
    fd_set read_fds;

    FD_ZERO(&read_fds);
    FD_SET(fifo_fd, &read_fds);

    if (pselect(fifo_fd + 1, &read_fds, NULL, NULL, NULL, sigset) < 0) {
        fprintf(stderr, WARN_LOG_PREFIX "pselect() error: ");
        perror(NULL);
        return -1;
    }

    if (!FD_ISSET(fifo_fd, &read_fds)) {
        fprintf(stderr, WARN_LOG_PREFIX "FIFO fd not in read fdset\n");
        return -1;
    }

    int nread = read(fifo_fd, fifo_buffer, FIFO_BUFFER_LEN);
    if (nread < 0) {
        perror(ERROR_LOG_PREFIX "read() from FIFO failed");
        return -1;
    } else if (nread == 0) {
        fprintf(stderr, WARN_LOG_PREFIX "read() from FIFO received EOF\n");
        return nread;
    }

    return nread;
}

/* commands */

int parse_command(char *in, char *out, int *button) {
    /* indeces for in/out */
    int i = 0, o = 0;

    /* skipping any leading spaces */
    while (in[i] == ' ')
        i++;

    /* copy command til encounter '\0', newline or space */
    while (in[i] != '\0' && in[i] != '\n' && in[i] != ' ') {
        out[o] = in[i];
        i++;
        o++;
    }
    /* set ending for command */
    out[o] = '\0';

    /* set button to zero by default */
    *button = 0;
    /* if we read til space, it means next will be either button, trash, newline or \0 */
    if (in[i] == ' ') {
        i++;
        *button = strtol(in + i, NULL, 10);
    }

    /* shifting til newline or \0 if it's still not found */
    while (in[i] != '\0' && in[i] != '\n')
        i++;

    /* if we found newline - shift to the next symbol, it's either next command or \0 */
    if (in[i] == '\n') i++;

    /* return amount of bytes til new command */
    return i;
}

void handle_commands_from_fifo(char *fifo_buffer, char *block_name_buffer) {
    char *next_command = fifo_buffer;
    int button = 0;

    while (*next_command != '\0') {
        next_command += parse_command(next_command, block_name_buffer, &button);
        if (strlen(block_name_buffer)) {
            fprintf(stderr, DEBUG_LOG_PREFIX "FIFO command: '%s %d'\n", block_name_buffer, button);
        } else {
            fprintf(stderr, WARN_LOG_PREFIX "got empty command from FIFO\n");
            continue;
        }

        int block_index;
        if (is_number(block_name_buffer, FIFO_BUFFER_LEN)) {
            /* used for direct clicks on bar */
            block_index = atoi(block_name_buffer);
            if (block_index < 1 || block_index > (int)BLOCKS_AMOUNT) {
                fprintf(stderr, ERROR_LOG_PREFIX "invalid block index: %d\n", block_index);
                return;
            }
            /* input block starts with 1, not 0, so we decrease it */
            block_index--;
        } else {
            /* used for updates coming from pipe */
            block_index = get_block_index(block_name_buffer);
            if (block_index == -1) {
                fprintf(stderr, ERROR_LOG_PREFIX "invalid block name: %s\n", block_name_buffer);
                return;
            }
        }

        run_block_command(block_index, button);
        update_status_buffer();
    }
}

/* execution flow */

bool setup() {
    fprintf(stderr, INFO_LOG_PREFIX "FIFO file: %s\n", fifo_path);

    /* expand full blocks path in case it has '~' or any env variables */
    expand_blocks_path();

    /* create fifo */
    remove_fifo_if_exists();
    if (!create_fifo()) return false;

    /* start threads */
    if (!start_updater()) return false;
    if (!start_services()) return false;

    /* setup signals after starting threads to not mess with their sigmask*/
    if (!setup_signals()) return false;

    return true;
}

void cleanup() {
    fprintf(stderr, INFO_LOG_PREFIX "cleaning up...\n");

    /* sigprocmask for multithreaded app */
    pthread_sigmask(SIG_SETMASK, &default_sigset, 0);

    cleanup_services();
    cleanup_updater();

    remove_fifo_if_exists();

    pthread_mutex_destroy(&run_command_lock);
    pthread_mutex_destroy(&update_status_lock);
}

void run() {
    char fifo_buffer[FIFO_BUFFER_LEN + 1] = {0};
    char block_name_buffer[FIFO_BUFFER_LEN + 1] = {0};

    while (is_running) {
        fprintf(stderr, INFO_LOG_PREFIX "opening FIFO...\n");

        int fifo_fd = -1;
        if ((fifo_fd = open_fifo()) < 0) {
            fprintf(stderr, ERROR_LOG_PREFIX "failed to create FIFO");
            break;
        }

        fprintf(stderr, INFO_LOG_PREFIX "reading FIFO...\n");
        while (is_running) {
            reset_buffer(fifo_buffer, FIFO_BUFFER_LEN);
            reset_buffer(block_name_buffer, FIFO_BUFFER_LEN);

            if (read_from_fifo(fifo_fd, fifo_buffer, &default_sigset) <= 0) break;
            handle_commands_from_fifo(fifo_buffer, block_name_buffer);
        }

        fprintf(stderr, INFO_LOG_PREFIX "closing FIFO...\n");
        if (close(fifo_fd) < 0) {
            perror(ERROR_LOG_PREFIX "close() failed");
            break;
        }
    }
}

int main(int argc, char **argv) {
    if (argc == 2 && !strcmp("-v", argv[1]))
        die("dwmbar-" VERSION);
    else if (argc != 1)
        die("usage: dwm [-v]");

    if (!setup()) die("setup failed");

    run();
    cleanup();

    if (is_restart) execvp(argv[0], argv);

    return EXIT_SUCCESS;
}
