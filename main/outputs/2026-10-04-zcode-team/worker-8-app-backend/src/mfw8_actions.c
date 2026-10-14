/* mfw8_actions.c — see mfw8_actions.h for the delegation contract. */
#include "mfw8_actions.h"

#include <errno.h>
#include <fcntl.h>
#include <signal.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/stat.h>
#include <sys/wait.h>
#include <unistd.h>

static const char *const k_cancel_points[MFW8_CANCEL_POINTS] = {
    "pre-stage", "post-stage", "swap-journal", "rename1"
};

const char *mfw8_cancel_point(int i) {
    if (i < 0 || i >= MFW8_CANCEL_POINTS) return NULL;
    return k_cancel_points[i];
}

int mfw8_installer_present(const mfw8_actions_config *cfg) {
    struct stat st;
    if (!cfg || !cfg->installer_path || !*cfg->installer_path) return 0;
    if (stat(cfg->installer_path, &st)) return 0;
    return S_ISREG(st.st_mode) && access(cfg->installer_path, X_OK) == 0;
}

static int config_valid(const mfw8_actions_config *cfg) {
    return cfg && cfg->installer_path && *cfg->installer_path &&
           cfg->state_dir && *cfg->state_dir && cfg->roots_base && *cfg->roots_base;
}

/* vendor SERVICE.md error vocabulary; the installer's exit contract narrows to
 * exactly one of these per exit code (modinst.c:54-60). */
static void classify(int exit_code, mfw8_outcome *out) {
    out->exit_code = exit_code;
    out->cancelled = (exit_code == 5);
    if (exit_code == 0) {
        out->state = "succeeded";
        out->error = "";
    } else if (exit_code == 5) {
        out->state = "cancelled";
        out->error = "cancelled";
    } else if (exit_code == 2) {
        out->state = "failed";
        out->error = "bad_request";
    } else if (exit_code == 3) {
        out->state = "failed";
        out->error = "not_ready";
    } else if (exit_code == 4) {
        out->state = "failed";
        out->error = "fomod_invalid_choice";   /* SERVICE.md needs_choices class */
    } else {
        out->state = "failed";
        out->error = "io_error";
    }
}

/* read end of a pipe drained into outcome->output (bounded). */
static void drain_output(int fd, mfw8_outcome *out) {
    size_t used = 0;
    for (;;) {
        ssize_t r;
        if (used + 1 >= sizeof out->output) {
            /* bound exceeded: keep the head, discard the rest */
            char sink[512];
            r = read(fd, sink, sizeof sink);
            if (r <= 0) break;
            continue;
        }
        r = read(fd, out->output + used, sizeof out->output - 1 - used);
        if (r <= 0) break;
        used += (size_t)r;
    }
    out->output[used] = 0;
}

static int run_installer(const mfw8_actions_config *cfg, char *const args[],
                         const char *cancel_at, mfw8_outcome *out) {
    int pipefd[2];
    pid_t pid;
    int status = 0;
    if (!config_valid(cfg)) { out->error = "io_error"; snprintf(out->detail, sizeof out->detail, "bad config"); return -1; }
    if (!args || !args[0]) { out->error = "io_error"; snprintf(out->detail, sizeof out->detail, "no argv"); return -1; }
    if (cancel_at) {
        int known = 0, i;
        for (i = 0; i < MFW8_CANCEL_POINTS; ++i)
            if (!strcmp(cancel_at, k_cancel_points[i])) known = 1;
        if (!known) { out->error = "bad_request"; snprintf(out->detail, sizeof out->detail, "unknown cancel point"); return -1; }
    }
    if (pipe(pipefd)) { out->error = "io_error"; snprintf(out->detail, sizeof out->detail, "pipe: %s", strerror(errno)); return -1; }
    pid = fork();
    if (pid < 0) {
        close(pipefd[0]); close(pipefd[1]);
        out->error = "io_error";
        snprintf(out->detail, sizeof out->detail, "fork: %s", strerror(errno));
        return -1;
    }
    if (pid == 0) {
        /* child: stdout -> pipe, stderr suppressed (stdout carries the facts) */
        close(pipefd[0]);
        dup2(pipefd[1], STDOUT_FILENO);
        close(pipefd[1]);
        close(STDERR_FILENO);
        fcntl(STDOUT_FILENO, F_SETFD, 0);
        execv(cfg->installer_path, args);
        _exit(127);
    }
    close(pipefd[1]);
    drain_output(pipefd[0], out);
    close(pipefd[0]);
    while (waitpid(pid, &status, 0) < 0) {
        if (errno != EINTR) { out->error = "io_error"; return -1; }
    }
    if (WIFEXITED(status)) {
        classify(WEXITSTATUS(status), out);
        if (WEXITSTATUS(status) == 127) snprintf(out->detail, sizeof out->detail, "installer not executable");
    } else if (WIFSIGNALED(status)) {
        out->state = "failed";
        out->error = "io_error";
        out->exit_code = 1;
        snprintf(out->detail, sizeof out->detail, "installer killed by signal %d", WTERMSIG(status));
    }
    return 0;
}

/* Extract "key value" facts from installer stdout into detail (e.g. the
 * needs_choices group or the first blocker line). */
static void scan_detail(mfw8_outcome *out) {
    static const char *const markers[] = {
        "needs_choices ", "blocker ", "error ", "cancelled ", "warning "
    };
    char *line, *save = NULL;
    size_t i;
    if (!out->output[0]) return;
    for (line = strtok_r(out->output, "\n", &save); line; line = strtok_r(0, "\n", &save)) {
        for (i = 0; i < sizeof markers / sizeof markers[0]; ++i) {
            const char *hit = strstr(line, markers[i]);
            if (hit) {
                snprintf(out->detail, sizeof out->detail, "%s", hit);
                return;
            }
        }
    }
}

int mfw8_act_register(const mfw8_actions_config *cfg, const mfw8_register_spec *spec,
                      mfw8_outcome *out) {
    char *argv[16];
    int n = 0;
    if (!spec || !spec->id || !spec->title) { out->error = "bad_request"; return -1; }
    if (!config_valid(cfg)) { out->error = "io_error"; return -1; }
    argv[n++] = (char *)cfg->installer_path;
    argv[n++] = (char *)"--state";       argv[n++] = (char *)cfg->state_dir;
    argv[n++] = (char *)"--roots";       argv[n++] = (char *)cfg->roots_base;
    argv[n++] = (char *)"register-mod";
    argv[n++] = (char *)"--id";          argv[n++] = (char *)spec->id;
    argv[n++] = (char *)"--title";       argv[n++] = (char *)spec->title;
    if (spec->bundle_path) {
        argv[n++] = (char *)"--bundle";  argv[n++] = (char *)spec->bundle_path;
    }
    argv[n] = NULL;
    {
        int rc = run_installer(cfg, argv, NULL, out);
        if (!rc) scan_detail(out);
        return rc;
    }
}

int mfw8_act_prepare(const mfw8_actions_config *cfg, const mfw8_prepare_spec *spec,
                     mfw8_outcome *out) {
    char *argv[24];
    int n = 0;
    if (!spec || !spec->title || !spec->mods_csv) { out->error = "bad_request"; return -1; }
    argv[n++] = (char *)cfg->installer_path;
    argv[n++] = (char *)"--state";       argv[n++] = (char *)cfg->state_dir;
    argv[n++] = (char *)"--roots";       argv[n++] = (char *)cfg->roots_base;
    argv[n++] = (char *)"prepare";
    argv[n++] = (char *)"--title";       argv[n++] = (char *)spec->title;
    argv[n++] = (char *)"--mods";        argv[n++] = (char *)spec->mods_csv;
    if (spec->choices_csv) {
        argv[n++] = (char *)"--choices"; argv[n++] = (char *)spec->choices_csv;
    }
    argv[n] = NULL;
    {
        int rc = run_installer(cfg, argv, NULL, out);
        if (!rc) scan_detail(out);
        return rc;
    }
}

int mfw8_act_prepare_archive(const mfw8_actions_config *cfg, const mfw8_archive_spec *spec,
                             mfw8_outcome *out) {
    char *argv[24];
    int n = 0;
    if (!spec || !spec->title || !spec->archive_path || !spec->quarantine_dir) {
        out->error = "bad_request";
        return -1;
    }
    argv[n++] = (char *)cfg->installer_path;
    argv[n++] = (char *)"--state";       argv[n++] = (char *)cfg->state_dir;
    argv[n++] = (char *)"--roots";       argv[n++] = (char *)cfg->roots_base;
    argv[n++] = (char *)"prepare-archive";
    argv[n++] = (char *)"--title";       argv[n++] = (char *)spec->title;
    argv[n++] = (char *)"--archive";     argv[n++] = (char *)spec->archive_path;
    argv[n++] = (char *)"--quarantine";  argv[n++] = (char *)spec->quarantine_dir;
    if (spec->mod_id) {
        argv[n++] = (char *)"--mod";     argv[n++] = (char *)spec->mod_id;
    }
    argv[n] = NULL;
    {
        int rc = run_installer(cfg, argv, NULL, out);
        if (!rc) scan_detail(out);
        return rc;
    }
}

int mfw8_act_install(const mfw8_actions_config *cfg, const mfw8_install_spec *spec,
                     mfw8_outcome *out) {
    char *argv[28];
    int n = 0;
    if (!spec || !spec->title) { out->error = "bad_request"; return -1; }
    if (spec->cancel_at) {
        int known = 0, i;
        for (i = 0; i < MFW8_CANCEL_POINTS; ++i)
            if (!strcmp(spec->cancel_at, k_cancel_points[i])) known = 1;
        if (!known) { out->error = "bad_request"; snprintf(out->detail, sizeof out->detail, "unknown cancel point"); return -1; }
    }
    argv[n++] = (char *)cfg->installer_path;
    argv[n++] = (char *)"--state";       argv[n++] = (char *)cfg->state_dir;
    argv[n++] = (char *)"--roots";       argv[n++] = (char *)cfg->roots_base;
    argv[n++] = (char *)"install";
    argv[n++] = (char *)"--title";       argv[n++] = (char *)spec->title;
    if (spec->mods_csv) {
        argv[n++] = (char *)"--mods";    argv[n++] = (char *)spec->mods_csv;
    }
    if (spec->remove_csv) {
        argv[n++] = (char *)"--remove";  argv[n++] = (char *)spec->remove_csv;
    }
    if (spec->choices_csv) {
        argv[n++] = (char *)"--choices"; argv[n++] = (char *)spec->choices_csv;
    }
    if (spec->cancel_at) {
        argv[n++] = (char *)"--cancel-at"; argv[n++] = (char *)spec->cancel_at;
    }
    argv[n] = NULL;
    {
        int rc = run_installer(cfg, argv, spec->cancel_at, out);
        if (!rc) scan_detail(out);
        return rc;
    }
}

int mfw8_act_uninstall(const mfw8_actions_config *cfg, const char *title,
                       const char *remove_csv, mfw8_outcome *out) {
    mfw8_install_spec spec;
    memset(&spec, 0, sizeof spec);
    spec.title = title;
    spec.remove_csv = remove_csv;
    return mfw8_act_install(cfg, &spec, out);
}

int mfw8_act_recover(const mfw8_actions_config *cfg, mfw8_outcome *out) {
    char *argv[8];
    int n = 0;
    argv[n++] = (char *)cfg->installer_path;
    argv[n++] = (char *)"--state";       argv[n++] = (char *)cfg->state_dir;
    argv[n++] = (char *)"--roots";       argv[n++] = (char *)cfg->roots_base;
    argv[n++] = (char *)"recover";
    argv[n] = NULL;
    {
        int rc = run_installer(cfg, argv, NULL, out);
        if (!rc) scan_detail(out);
        return rc;
    }
}
