// cam-trigger — record an MP4 clip from Camera Module 3 whenever a *command*
// is typed on STDIN.
// Commands:
//   save            -> record one clip using --duration and --outdir
//   save 15000      -> record a 15s clip (one-off override)
//   status          -> print whether a recording is running
//   quit / exit     -> stop the program
//
// Build: CMake target "cam_trigger" (see CMakeLists.txt)
// Run:   ./cam_trigger --duration 10000 --outdir /mnt/ssd/clips --verbose

#define _POSIX_C_SOURCE 200809L

#include <errno.h>
#include <signal.h>
#include <stdarg.h>
#include <stdbool.h>
#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/stat.h>
#include <sys/types.h>
#include <sys/wait.h>
#include <time.h>
#include <unistd.h>

// ---------------------------- Config ----------------------------
typedef struct {
    int duration_ms;           // default clip length
    const char *out_dir;       // where to write clips
    int min_gap_ms;            // ignore triggers that occur within N ms of the previous one
    bool verbose;              // extra logs
} app_config_t;

static app_config_t cfg = {
    .duration_ms = 10000,
    .out_dir = "/mnt/ssd/clips",
    .min_gap_ms = 500,
    .verbose = false,
};

// ---------------------------- Globals ----------------------------
static volatile sig_atomic_t g_stop = 0;
static volatile sig_atomic_t g_children = 0;  // number of active recorder children

// ---------------------------- Helpers ----------------------------
static void logf(const char *fmt, ...) {
    va_list ap; va_start(ap, fmt);
    vfprintf(stderr, fmt, ap);
    fprintf(stderr, "\n");
    va_end(ap);
}

static void vlogf(bool cond, const char *fmt, ...) {
    if (!cond) return;
    va_list ap; va_start(ap, fmt);
    vfprintf(stderr, fmt, ap);
    fprintf(stderr, "\n");
    va_end(ap);
}

static int ensure_dir(const char *path) {
    struct stat st;
    if (stat(path, &st) == 0) {
        if (S_ISDIR(st.st_mode)) return 0;
        errno = ENOTDIR; return -1;
    }
    if (mkdir(path, 0775) == 0) return 0;
    return -1;
}

static void timestamp_yyyymmdd_hhmmss(char *buf, size_t buflen) {
    time_t t = time(NULL);
    struct tm tm;
    localtime_r(&t, &tm);
    strftime(buf, buflen, "%Y%m%d-%H%M%S", &tm);
}

// Spawn rpicam-vid to record one clip. Returns 0 on success (parent), never returns in child.
static int spawn_recorder(int duration_ms, const char *out_dir, bool verbose) {
    char ts[32];
    timestamp_yyyymmdd_hhmmss(ts, sizeof ts);

    char filepath[512];
    snprintf(filepath, sizeof filepath, "%s/clip-%s.mp4", out_dir, ts);

    char dur[32]; snprintf(dur, sizeof dur, "%d", duration_ms);

    const char *argv[] = {
        "rpicam-vid",
        "-n",
        "-t", dur,
        "-o", filepath,
        NULL
    };

    pid_t pid = fork();
    if (pid < 0) {
        logf("ERROR: fork() failed: %s", strerror(errno));
        return -1;
    }
    if (pid == 0) {
        if (verbose) {
            fprintf(stderr, "[child] exec: rpicam-vid -n -t %s -o %s\n", dur, filepath);
        }
        execvp(argv[0], (char * const *)argv);
        fprintf(stderr, "ERROR: execvp(rpicam-vid) failed: %s\n", strerror(errno));
        _exit(127);
    }

    g_children++;
    vlogf(verbose, "spawned recorder pid=%d", (int)pid);
    return 0;
}

static void on_sigint(int signo) { (void)signo; g_stop = 1; }

static void on_sigchld(int signo) {
    (void)signo;
    int saved = errno;
    int status;
    pid_t pid;
    while ((pid = waitpid(-1, &status, WNOHANG)) > 0) {
        if (g_children > 0) g_children--;
    }
    errno = saved;
}

static void usage(const char *prog) {
    fprintf(stderr,
        "Usage: %s [--duration ms] [--outdir PATH] [--min-gap ms] [--verbose]\n\n"
        "Commands on STDIN: save [ms] | status | quit\n\n"
        "Defaults:\n  --duration %d\n  --outdir %s\n  --min-gap %d\n\n",
        prog, cfg.duration_ms, cfg.out_dir, cfg.min_gap_ms);
}

static char *trim(char *s) {
    if (!s) return s;
    while (*s && (*s==' '||*s=='\t'||*s=='\n'||*s=='\r')) s++;
    size_t len = strlen(s);
    while (len && (s[len-1]==' '||s[len-1]=='\t'||s[len-1]=='\n'||s[len-1]=='\r')) s[--len]='\0';
    return s;
}

// ---------------------------- Main ----------------------------
int main(int argc, char **argv) {
    // CLI flags
    for (int i = 1; i < argc; ++i) {
        if (!strcmp(argv[i], "--duration") && i+1 < argc) { cfg.duration_ms = atoi(argv[++i]); }
        else if (!strcmp(argv[i], "--outdir") && i+1 < argc) { cfg.out_dir = argv[++i]; }
        else if (!strcmp(argv[i], "--min-gap") && i+1 < argc) { cfg.min_gap_ms = atoi(argv[++i]); }
        else if (!strcmp(argv[i], "--verbose")) { cfg.verbose = true; }
        else if (!strcmp(argv[i], "-h") || !strcmp(argv[i], "--help")) { usage(argv[0]); return 0; }
        else { fprintf(stderr, "Unknown arg: %s\n", argv[i]); usage(argv[0]); return 1; }
    }

    if (ensure_dir(cfg.out_dir) != 0) {
        logf("ERROR: cannot create/access outdir '%s': %s", cfg.out_dir, strerror(errno));
        return 1;
    }

    // Signals
    struct sigaction sa_int = {0}; sa_int.sa_handler = on_sigint; sigemptyset(&sa_int.sa_mask);
    sigaction(SIGINT, &sa_int, NULL); sigaction(SIGTERM, &sa_int, NULL);
    struct sigaction sa_chld = {0}; sa_chld.sa_handler = on_sigchld; sigemptyset(&sa_chld.sa_mask);
    sa_chld.sa_flags = SA_RESTART | SA_NOCLDSTOP; sigaction(SIGCHLD, &sa_chld, NULL);

    logf("Command-trigger mode. Type 'save', 'status', or 'quit'.");
    logf("Default 'save': %d ms to %s", cfg.duration_ms, cfg.out_dir);

    // Command loop on STDIN
    char *line = NULL; size_t cap = 0; ssize_t nread;
    long last_ms_since_start = -1000000; // allow immediate first trigger
    struct timespec prog_start; clock_gettime(CLOCK_MONOTONIC, &prog_start);

    while (!g_stop && (nread = getline(&line, &cap, stdin)) != -1) {
        char *cmd = trim(line);
        if (*cmd == '\0') continue;

        if (!strncmp(cmd, "quit", 4) || !strncmp(cmd, "exit", 4)) {
            logf("Exiting on user command.");
            break;
        } else if (!strncmp(cmd, "status", 6)) {
            logf("Status: %s", g_children > 0 ? "recording" : "idle");
            continue;
        } else if (!strncmp(cmd, "save", 4)) {
            int dur = cfg.duration_ms;
            char *arg = cmd + 4;
            while (*arg == ' ' || *arg == '\t') arg++;
            if (*arg) {
                int tmp = atoi(arg);
                if (tmp > 0) dur = tmp;
            }

            struct timespec now; clock_gettime(CLOCK_MONOTONIC, &now);
            long ms_since_start = (now.tv_sec - prog_start.tv_sec) * 1000L +
                                  (now.tv_nsec - prog_start.tv_nsec) / 1000000L;
            if (ms_since_start - last_ms_since_start < cfg.min_gap_ms) {
                vlogf(cfg.verbose, "debounce: ignored (gap < %d ms)", cfg.min_gap_ms);
                continue;
            }
            last_ms_since_start = ms_since_start;

            if (g_children > 0) {
                vlogf(cfg.verbose, "ignored: a recording is already running");
                continue;
            }

            if (spawn_recorder(dur, cfg.out_dir, cfg.verbose) != 0) {
                logf("ERROR: failed to start recorder");
            }
            continue;
        } else if (!strncmp(cmd, "help", 4) || !strncmp(cmd, "?", 1)) {
            logf("Commands: save [ms], status, quit");
            continue;
        } else {
            logf("Unknown command: %s", cmd);
            logf("Type: save [ms] | status | quit");
        }
    }

    free(line);

    // Wait for any child to finish
    while (g_children > 0) {
        int status; if (waitpid(-1, &status, 0) > 0) { g_children--; }
    }
    return 0;
}
