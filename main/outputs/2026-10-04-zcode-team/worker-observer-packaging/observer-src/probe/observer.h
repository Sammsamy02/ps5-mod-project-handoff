#ifndef ALIGNED_ENTRY_OBSERVER_H
#define ALIGNED_ENTRY_OBSERVER_H

/* Independent UE4SS-presence observer primitives for the aligned-entry
   lineage. Read-only by contract: stat + bounded pread/read + close only.
   No lseek, no writes to observed paths, no UE4SS memory access.
   This unit is deliberately free of diagnostic logging so it can be
   host-tested against shims without any console dependencies. */

#include <stddef.h>
#include <stdint.h>

enum observer_growth {
    OBSERVER_SAME = 0,
    OBSERVER_GREW = 1,
    OBSERVER_SHRANK = 2
};

enum observer_tail_result {
    OBSERVER_TAIL_OK = 0,
    OBSERVER_TAIL_ERROR = -1,       /* open/pread/read failure this poll */
    OBSERVER_TAIL_DRAIN_CAPPED = -2 /* file larger than drain cap (read path) */
};

/* stat() once. Returns 0 and fills size_out when the path exists.
   is_dir_out (nullable) reports S_ISDIR(st_mode). Returns -1 on error. */
int observer_stat_path(const char *path, int64_t *size_out, int *is_dir_out);

/* Existence-only probe. Returns 1 exists, 0 absent, -1 stat error.
   is_dir_out (nullable) reports S_ISDIR when the path exists. */
int observer_check_exists(const char *path, int *is_dir_out);

/* Pure classification of a size transition. */
enum observer_growth observer_classify(int64_t previous, int64_t current);

/* Read up to cap bytes of path ending at file_size (the size just observed).
   Opens the path fresh (O_RDONLY | O_NOFOLLOW), reads without repositioning
   the file offset of any other reader: primary path uses one pread at
   offset max(0, file_size - cap); if pread fails, falls back to a bounded
   forward drain using read() capped at drain_limit bytes total.
   Returns OBSERVER_TAIL_OK with *len_out set (may be < requested at EOF),
   OBSERVER_TAIL_ERROR, or OBSERVER_TAIL_DRAIN_CAPPED. */
int observer_read_tail(const char *path, int64_t file_size, unsigned char *buf,
                       size_t cap, size_t *len_out, size_t drain_limit);

/* Pure last-line extraction over a tail window. The last COMPLETE line is
   reported: one trailing LF (plus a preceding CR) is treated as the final
   line's terminator; without a trailing LF the final partial line is used.
   Sanitizes: keeps printable ASCII 0x20..0x7E, maps CR and other
   non-printables to '?', always NUL-terminates, and never writes more than
   cap bytes into out.
   window_at_start: 1 when the tail window began at file offset 0.
   *truncated_out (nullable) set to 1 when the returned text may not be the
   complete final line (fragment cut by the window start with no in-window
   line boundary, or capped by out capacity). Returns sanitized length. */
size_t observer_last_line(const unsigned char *buf, size_t len,
                          int window_at_start, char *out, size_t cap,
                          int *truncated_out);

#endif
