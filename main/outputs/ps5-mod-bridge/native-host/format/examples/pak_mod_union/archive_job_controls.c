#define _DARWIN_C_SOURCE 1
#define _POSIX_C_SOURCE 200809L

#include "../../pak_archive_job.h"

#include <dirent.h>
#include <errno.h>
#include <fcntl.h>
#include <inttypes.h>
#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/stat.h>
#include <unistd.h>

/* Ordinary-host integration controls for the callable archive job. All paths
 * are harness inputs/setup; the job itself receives only borrowed descriptors. */
static unsigned checks;

static void require(int ok, const char *message) {
    ++checks;
    if (!ok) {
        fprintf(stderr, "archive job control: %s\n", message);
        exit(1);
    }
}

static int hex_value(unsigned char c) {
    if (c >= '0' && c <= '9') return c - '0';
    if (c >= 'a' && c <= 'f') return c - 'a' + 10;
    if (c >= 'A' && c <= 'F') return c - 'A' + 10;
    return -1;
}

static int parse_hex32(const char *text, uint8_t out[32]) {
    if (!text || strlen(text) != 64u) return 0;
    for (unsigned i = 0; i < 32u; ++i) {
        int hi = hex_value((unsigned char)text[i * 2u]);
        int lo = hex_value((unsigned char)text[i * 2u + 1u]);
        if (hi < 0 || lo < 0) return 0;
        out[i] = (uint8_t)((unsigned)hi * 16u + (unsigned)lo);
    }
    return 1;
}

static uint64_t file_size(int fd) {
    struct stat st;
    require(fstat(fd, &st) == 0 && st.st_size >= 0, "fstat failed");
    return (uint64_t)st.st_size;
}

static int count_entries(int dirfd) {
    int copy = dup(dirfd);
    DIR *dir;
    struct dirent *entry;
    int count = -1;
    require(copy >= 0, "dup directory failed");
    dir = fdopendir(copy);
    require(dir != NULL, "fdopendir failed");
    /* dup shares the open directory cursor, which the archive job leaves
     * unspecified after materialization. Restart before counting entries. */
    rewinddir(dir);
    count = 0;
    errno = 0;
    while ((entry = readdir(dir)) != NULL) {
        if (strcmp(entry->d_name, ".") && strcmp(entry->d_name, "..")) ++count;
    }
    require(errno == 0, "readdir failed");
    require(closedir(dir) == 0, "closedir failed");
    return count;
}

enum output_mode { OUTPUT_FRESH = 0, OUTPUT_READONLY, OUTPUT_NONEMPTY,
                   OUTPUT_APPEND, OUTPUT_PIPE };

typedef struct output_context {
    int directory_fd;
    int quarantine_fd;
    enum output_mode mode;
    unsigned calls;
    unsigned saw_materialized_rows;
    int retained_pipe_write_fd;
    off_t cursor_after_acquire;
    char name[64];
} output_context;

static int acquire_output(void *opaque, int *fd_out) {
    output_context *ctx = (output_context *)opaque;
    int fd = -1;
    ++ctx->calls;
    *fd_out = -1;
    ctx->saw_materialized_rows = count_entries(ctx->quarantine_fd) >= 4;
    if (!ctx->saw_materialized_rows) return -1;
    if (ctx->mode == OUTPUT_PIPE) {
        int pipefd[2];
        if (pipe(pipefd) != 0) return -1;
        ctx->retained_pipe_write_fd = pipefd[1];
        *fd_out = pipefd[0];
        return 0;
    }
    if (ctx->mode == OUTPUT_READONLY) {
        fd = openat(ctx->directory_fd, ctx->name,
                    O_RDONLY | O_CREAT | O_EXCL | O_NOFOLLOW | O_CLOEXEC, 0600);
    } else {
        int flags = O_RDWR | O_CREAT | O_EXCL | O_NOFOLLOW | O_CLOEXEC;
        if (ctx->mode == OUTPUT_APPEND) flags |= O_APPEND;
        fd = openat(ctx->directory_fd, ctx->name, flags, 0600);
    }
    if (fd < 0) return -1;
    if (ctx->mode == OUTPUT_NONEMPTY && write(fd, "keep", 4u) != (ssize_t)4) {
        (void)close(fd);
        return -1;
    }
    ctx->cursor_after_acquire = lseek(fd, 0, SEEK_CUR);
    *fd_out = fd;
    return 0;
}

typedef struct case_env {
    int root_fd, quarantine_fd, base_fd, source_fd;
    uint64_t base_bytes, source_bytes;
    mb_pak_archive_job_params_v1 params;
    output_context output;
} case_env;

static int make_case_dir(int root_fd, const char *name) {
    require(mkdirat(root_fd, name, 0700) == 0, "case directory creation failed");
    int fd = openat(root_fd, name, O_RDONLY | O_DIRECTORY | O_NOFOLLOW | O_CLOEXEC);
    require(fd >= 0, "case directory open failed");
    return fd;
}

static void init_case(case_env *env, int root_fd, const char *name,
                      const char *base_path, const char *source_path,
                      const uint8_t source_hash[32], const uint8_t target[32],
                      enum output_mode mode) {
    char qname[96];
    memset(env, 0, sizeof(*env));
    env->root_fd = make_case_dir(root_fd, name);
    require(snprintf(qname, sizeof(qname), "%s-q", name) > 0, "quarantine name failed");
    require(mkdirat(env->root_fd, qname, 0700) == 0, "quarantine creation failed");
    env->quarantine_fd = openat(env->root_fd, qname,
                                O_RDONLY | O_DIRECTORY | O_NOFOLLOW | O_CLOEXEC);
    env->base_fd = open(base_path, O_RDONLY | O_NOFOLLOW | O_CLOEXEC);
    env->source_fd = open(source_path, O_RDONLY | O_NOFOLLOW | O_CLOEXEC);
    require(env->quarantine_fd >= 0 && env->base_fd >= 0 && env->source_fd >= 0,
            "fixture descriptor open failed");
    env->base_bytes = file_size(env->base_fd);
    env->source_bytes = file_size(env->source_fd);

    env->output.directory_fd = env->root_fd;
    env->output.quarantine_fd = env->quarantine_fd;
    env->output.mode = mode;
    env->output.retained_pipe_write_fd = -1;
    require(snprintf(env->output.name, sizeof(env->output.name), "%s-output.pak", name) > 0,
            "output name failed");

    memset(&env->params, 0, sizeof(env->params));
    env->params.struct_size = sizeof(env->params);
    env->params.abi_version = MB_PAK_ARCHIVE_JOB_ABI_V1;
    env->params.source.struct_size = sizeof(env->params.source);
    env->params.source.abi_version = MB_ARCHIVE_ABI_V1;
    env->params.source.fd = env->source_fd;
    env->params.source.size = env->source_bytes;
    memcpy(env->params.source.sha256, source_hash, 32u);
    memcpy(env->params.source.target_token, target, 32u);
    env->params.base_fd = env->base_fd;
    env->params.quarantine_fd = env->quarantine_fd;
    env->params.output_fd = -1;
    env->params.immutable_inputs = 1u;
    env->params.private_quarantine = 1u;
    env->params.unpublished_output = 1u;
    env->params.policy = MB_PAK_ARCHIVE_POLICY_PAK_COMPONENT_ONLY;
    env->params.base_bytes = env->base_bytes;
    env->params.acquire_output = acquire_output;
    env->params.output_context = &env->output;
}

static mb_pak_archive_job_workspace_v1 *fresh_workspace(void) {
    mb_pak_archive_job_workspace_v1 *workspace =
        (mb_pak_archive_job_workspace_v1 *)calloc(1u, sizeof(*workspace));
    require(workspace != NULL, "workspace allocation failed");
    workspace->struct_size = sizeof(*workspace);
    workspace->abi_version = MB_PAK_ARCHIVE_JOB_ABI_V1;
    return workspace;
}

static void cursor_at(int fd, off_t position) {
    require(lseek(fd, position, SEEK_SET) == position, "fixture seek failed");
}

static void check_borrowed(const case_env *env, off_t source_pos, off_t base_pos) {
    require(fcntl(env->source_fd, F_GETFD) >= 0 && fcntl(env->base_fd, F_GETFD) >= 0 &&
            fcntl(env->quarantine_fd, F_GETFD) >= 0, "borrowed descriptor was closed");
    require(lseek(env->source_fd, 0, SEEK_CUR) == source_pos &&
            lseek(env->base_fd, 0, SEEK_CUR) == base_pos,
            "source or base cursor changed");
}

static int same_bytes(int left, int right) {
    uint8_t a[4096], b[4096];
    uint64_t size = file_size(left);
    if (file_size(right) != size) return 0;
    for (uint64_t offset = 0; offset < size;) {
        size_t amount = (size - offset < sizeof(a)) ? (size_t)(size - offset) : sizeof(a);
        ssize_t na = pread(left, a, amount, (off_t)offset);
        ssize_t nb = pread(right, b, amount, (off_t)offset);
        if (na != (ssize_t)amount || nb != (ssize_t)amount || memcmp(a, b, amount)) return 0;
        offset += amount;
    }
    return 1;
}

static void close_case(case_env *env) {
    if (env->output.retained_pipe_write_fd >= 0)
        require(close(env->output.retained_pipe_write_fd) == 0, "pipe cleanup failed");
    if (env->source_fd >= 0) require(close(env->source_fd) == 0, "source close failed");
    if (env->base_fd >= 0) require(close(env->base_fd) == 0, "base close failed");
    if (env->quarantine_fd >= 0) require(close(env->quarantine_fd) == 0, "quarantine close failed");
    if (env->root_fd >= 0) require(close(env->root_fd) == 0, "case directory close failed");
}

static mb_pak_archive_job_status_v1 run_job(case_env *env,
        mb_pak_archive_job_workspace_v1 *workspace,
        mb_pak_archive_job_result_v1 *result) {
    memset(result, 0, sizeof(*result));
    return mb_pak_archive_job_prepare_v1(&env->params, workspace, result);
}

static void test_default_refusal(int root_fd, const char *base_path,
                                 const char *source_path, const uint8_t source_hash[32],
                                 const uint8_t target[32]) {
    case_env env;
    mb_pak_archive_job_result_v1 result;
    mb_pak_archive_job_workspace_v1 *workspace;
    init_case(&env, root_fd, "default-extras", base_path, source_path,
              source_hash, target, OUTPUT_FRESH);
    env.params.policy = MB_PAK_ARCHIVE_POLICY_DEFAULT;
    workspace = fresh_workspace();
    mb_pak_archive_job_status_v1 status = run_job(&env, workspace, &result);
    require(status == MB_PAK_ARCHIVE_JOB_ADDITIONAL_ACTION_REQUIRED &&
            !result.preparation_complete && result.selection_called &&
            result.selection.retained_extra_count == 2u,
            "default policy did not refuse extras with diagnostics");
    require(env.output.calls == 0u && count_entries(env.quarantine_fd) == 0,
            "default refusal acquired output or materialized members");
    free(workspace);
    close_case(&env);
}

static int test_component_success(int root_fd, const char *case_name,
                                  const char *base_path, const char *source_path,
                                  const uint8_t source_hash[32], const uint8_t target[32],
                                  int previous_output_fd) {
    case_env env;
    mb_pak_archive_job_result_v1 result;
    mb_pak_archive_job_workspace_v1 *workspace;
    init_case(&env, root_fd, case_name, base_path, source_path,
              source_hash, target, OUTPUT_FRESH);
    workspace = fresh_workspace();
    cursor_at(env.source_fd, 13);
    cursor_at(env.base_fd, 17);
    mb_pak_archive_job_status_v1 status = run_job(&env, workspace, &result);
    if (status != MB_PAK_ARCHIVE_JOB_OK || !result.preparation_complete ||
        !result.output_acquired || !result.source_stable) {
        fprintf(stderr, "component failure: status=%u stage=%s error=%s callback_calls=%u\n",
                (unsigned)status, mb_pak_archive_job_stage_name_v1(result.stage),
                result.error, env.output.calls);
    }
    require(status == MB_PAK_ARCHIVE_JOB_OK && result.preparation_complete &&
            result.output_acquired && result.source_stable,
            "component archive job did not complete");
    require(result.selection.member_list_complete && result.selection.member_count == 3u &&
            result.selection.retained_extra_count == 2u &&
            result.materialization_complete,
            "successful job omitted regular member metadata");
    {
        uint32_t seen = 0u, pak_rows = 0u, extra_rows = 0u;
        require(result.inventory.complete && result.inventory.row_count == 3u,
                "mixed fixture inventory does not contain all regular rows");
        for (uint32_t i = 0; i < result.selection.member_count; ++i) {
            const mb_pak_archive_member_v1 *member = workspace->members + i;
            require(member->row_index < result.inventory.row_count &&
                    (seen & (1u << member->row_index)) == 0u,
                    "member metadata has an invalid or duplicate row");
            seen |= 1u << member->row_index;
            require(member->bytes == workspace->rows[member->row_index].size,
                    "member byte count differs from inventory");
            require(memcmp(member->sha256, workspace->rows[member->row_index].sha256, 32u) == 0,
                    "member digest differs from inventory");
            if (member->role == MB_PAK_ARCHIVE_ROLE_PAK) ++pak_rows;
            else if (member->role == MB_PAK_ARCHIVE_ROLE_RETAINED_EXTRA &&
                     member->retained_unhandled) ++extra_rows;
            else require(0, "regular member has no explicit preparation role");
        }
        require(seen == 7u && pak_rows == 1u && extra_rows == 2u,
                "successful job did not retain each regular row exactly once");
    }
    require(result.installation_supported == 0u && result.runtime_supported == 0u &&
            !result.selection.installation_supported && !result.selection.runtime_supported,
            "preparation claimed installation or runtime support");
    require(env.output.calls == 1u && result.output_fd >= 0 &&
            env.output.saw_materialized_rows &&
            fcntl(result.output_fd, F_GETFD) >= 0 && file_size(result.output_fd) > 0u,
            "successful output callback or artifact invalid");
    require(lseek(result.output_fd, 0, SEEK_CUR) == 0,
            "output callback cursor changed");
    if (previous_output_fd >= 0)
        require(same_bytes(previous_output_fd, result.output_fd),
                "fresh same-process job output differs from prior successful run");
    check_borrowed(&env, 13, 17);
    require(count_entries(env.quarantine_fd) >= 4,
            "successful job did not retain member and manifest rows");
    int output_fd = result.output_fd;
    free(workspace);
    close_case(&env);
    return output_fd;
}

static void test_source_refusal(int root_fd, const char *case_name,
                                const char *base_path, const char *source_path,
                                const uint8_t source_hash[32], const uint8_t target[32],
                                int bad_length) {
    case_env env;
    mb_pak_archive_job_result_v1 result;
    mb_pak_archive_job_workspace_v1 *workspace;
    init_case(&env, root_fd, case_name, base_path, source_path,
              source_hash, target, OUTPUT_FRESH);
    if (bad_length) ++env.params.source.size;
    else {
        int writable = open(source_path, O_RDWR | O_NOFOLLOW | O_CLOEXEC);
        require(writable >= 0, "writable source control open failed");
        require(close(env.source_fd) == 0, "source descriptor replace close failed");
        env.source_fd = writable;
        env.params.source.fd = writable;
    }
    workspace = fresh_workspace();
    mb_pak_archive_job_status_v1 status = run_job(&env, workspace, &result);
    require(status != MB_PAK_ARCHIVE_JOB_OK && env.output.calls == 0u,
            bad_length ? "declared source length mismatch was admitted" :
                         "writable source was admitted");
    require(count_entries(env.quarantine_fd) == 0 && result.output_acquired == 0u,
            "invalid source reached materialization/output acquisition");
    free(workspace);
    close_case(&env);
}

static void test_bad_output(int root_fd, const char *case_name,
                            const char *base_path, const char *source_path,
                            const uint8_t source_hash[32], const uint8_t target[32],
                            enum output_mode mode) {
    case_env env;
    mb_pak_archive_job_result_v1 result;
    mb_pak_archive_job_workspace_v1 *workspace;
    init_case(&env, root_fd, case_name, base_path, source_path,
              source_hash, target, mode);
    workspace = fresh_workspace();
    mb_pak_archive_job_status_v1 status = run_job(&env, workspace, &result);
    require(status == MB_PAK_ARCHIVE_JOB_OUTPUT_REFUSED && env.output.calls == 1u &&
            env.output.saw_materialized_rows && result.output_acquired &&
            !result.preparation_complete,
            "invalid callback output was not refused");
    require(result.output_fd >= 0 && fcntl(result.output_fd, F_GETFD) >= 0,
            "acquired descriptor was closed by the job");
    if (mode == OUTPUT_PIPE) {
        require(write(env.output.retained_pipe_write_fd, "x", 1u) == (ssize_t)1,
                "acquired pipe remains caller-owned");
        require(close(result.output_fd) == 0, "caller pipe read-end close failed");
    } else {
        require(file_size(result.output_fd) == (mode == OUTPUT_NONEMPTY ? 4u : 0u),
                "refused output length changed");
        require(lseek(result.output_fd, 0, SEEK_CUR) == env.output.cursor_after_acquire,
                "refused output cursor changed");
        if (mode == OUTPUT_NONEMPTY) {
            char value[4];
            require(pread(result.output_fd, value, sizeof(value), 0) == 4 &&
                    memcmp(value, "keep", 4u) == 0,
                    "preexisting output bytes changed");
        }
        require(close(result.output_fd) == 0, "caller-owned invalid output close failed");
    }
    free(workspace);
    close_case(&env);
}

static void test_journal_preflight(int root_fd, const char *base_path,
                                   const char *source_path, const uint8_t source_hash[32],
                                   const uint8_t target[32]) {
    case_env env;
    mb_pak_archive_job_result_v1 result, before_result;
    mb_pak_archive_job_workspace_v1 *workspace;
    mb_archive_owned_file_v1 before_record;
    init_case(&env, root_fd, "nonfresh-journal", base_path, source_path,
              source_hash, target, OUTPUT_FRESH);
    workspace = fresh_workspace();
    memset(&result, 0x5a, sizeof(result));
    before_result = result;
    memset(&workspace->journal[0], 0, sizeof(workspace->journal[0]));
    memcpy(workspace->journal[0].name, "recovery-record", sizeof("recovery-record"));
    workspace->journal[0].state = MB_ARCHIVE_RETAINED;
    workspace->journal[0].row_index = 19u;
    workspace->journal[0].written_bytes = UINT64_C(0x12345678);
    before_record = workspace->journal[0];
    mb_pak_archive_job_status_v1 status = mb_pak_archive_job_prepare_v1(
        &env.params, workspace, &result);
    require(status == MB_PAK_ARCHIVE_JOB_BAD_ARGUMENT &&
            memcmp(&result, &before_result, sizeof(result)) == 0,
            "nonfresh journal changed the result during preflight");
    require(memcmp(&workspace->journal[0], &before_record, sizeof(before_record)) == 0 &&
            env.output.calls == 0u && count_entries(env.quarantine_fd) == 0,
            "nonfresh journal recovery record was cleared or admitted");
    free(workspace);
    close_case(&env);
}

int main(int argc, char **argv) {
    uint8_t source_hash[32], target[32];
    int base_check, source_check, root_fd;
    if (argc != 6) {
        fprintf(stderr, "usage: archive_job_controls BASE MIXEDZIP ARCHIVE_SHA256 TARGET_HEX FRESH_DIR\n");
        return 2;
    }
    require(parse_hex32(argv[3], source_hash) && parse_hex32(argv[4], target),
            "invalid source digest or target token hex");
    base_check = open(argv[1], O_RDONLY | O_NOFOLLOW | O_CLOEXEC);
    source_check = open(argv[2], O_RDONLY | O_NOFOLLOW | O_CLOEXEC);
    require(base_check >= 0 && source_check >= 0, "fixture open failed");
    require(close(base_check) == 0 && close(source_check) == 0, "fixture close failed");
    require(mkdir(argv[5], 0700) == 0, "fresh harness directory required");
    root_fd = open(argv[5], O_RDONLY | O_DIRECTORY | O_NOFOLLOW | O_CLOEXEC);
    require(root_fd >= 0, "fresh harness directory open failed");

    test_default_refusal(root_fd, argv[1], argv[2], source_hash, target);
    int first_output = test_component_success(root_fd, "component-success-a", argv[1], argv[2],
                                              source_hash, target, -1);
    int second_output = test_component_success(root_fd, "component-success-b", argv[1], argv[2],
                                               source_hash, target, first_output);
    require(close(first_output) == 0 && close(second_output) == 0,
            "caller-owned successful output close failed");
    test_source_refusal(root_fd, "writable-source", argv[1], argv[2],
                        source_hash, target, 0);
    test_source_refusal(root_fd, "wrong-source-length", argv[1], argv[2],
                        source_hash, target, 1);
    test_bad_output(root_fd, "readonly-output", argv[1], argv[2], source_hash,
                    target, OUTPUT_READONLY);
    test_bad_output(root_fd, "nonempty-output", argv[1], argv[2], source_hash,
                    target, OUTPUT_NONEMPTY);
    test_bad_output(root_fd, "append-output", argv[1], argv[2], source_hash,
                    target, OUTPUT_APPEND);
    test_bad_output(root_fd, "invalid-output-fd", argv[1], argv[2], source_hash,
                    target, OUTPUT_PIPE);
    test_journal_preflight(root_fd, argv[1], argv[2], source_hash, target);

    {
        char receipt[1024];
        int length = snprintf(receipt, sizeof(receipt),
            "{\"status\":\"pass\",\"cases\":["
            "\"default-extras-no-output\",\"component-success-a\","
            "\"component-success-b\",\"writable-source\","
            "\"wrong-source-length\",\"readonly-output\","
            "\"nonempty-output\",\"append-output\",\"invalid-output-fd\","
            "\"nonfresh-journal\"],\"fresh_workspaces_retain_rows\":true,"
            "\"borrowed_fds_preserved\":true,\"callback_contracts_checked\":true,"
            "\"journal_recovery_preserved\":true,\"console_actions\":false}\n");
        int receipt_fd;
        require(length > 0 && (size_t)length < sizeof(receipt), "receipt formatting failed");
        receipt_fd = openat(root_fd, "receipt.json",
                            O_WRONLY | O_CREAT | O_EXCL | O_NOFOLLOW | O_CLOEXEC, 0600);
        require(receipt_fd >= 0, "receipt creation failed");
        require(write(receipt_fd, receipt, (size_t)length) == (ssize_t)length,
                "receipt write failed");
        require(close(receipt_fd) == 0, "receipt close failed");
    }
    require(close(root_fd) == 0, "harness directory close failed");
    printf("{\"status\":\"pass\",\"checks\":%u,\"cases\":10,"
           "\"fresh_workspaces_retain_rows\":true,\"borrowed_fds_preserved\":true,"
           "\"callback_contracts_checked\":true,\"journal_recovery_preserved\":true,"
           "\"console_actions\":false}\n", checks);
    return 0;
}
