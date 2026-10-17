#include "pe_file_source.h"
#include <errno.h>
#include <fcntl.h>
#include <stdlib.h>
#include <sys/stat.h>
#include <unistd.h>
int mb_pe_file_source_acquire(void *context,
    const mb_pe_loader_catalog_entry *entry, const void **file,
    size_t *size, void **token) {
    const mb_pe_file_source_profile *profile;
    struct stat st;
    unsigned char *snapshot = NULL;
    size_t bytes, used = 0;
    int fd, rc, result = MB_PE_FILE_SOURCE_OK;
    (void)context;
    if (file) *file = NULL;
    if (size) *size = 0;
    if (token) *token = NULL;
    if (!file || !size || !token || !entry || !entry->source_context)
        return MB_PE_FILE_SOURCE_INVALID_ARGUMENT;
    profile = (const mb_pe_file_source_profile *)entry->source_context;
    if (!profile->path || !profile->path[0] || !profile->max_bytes ||
        profile->max_bytes > MB_PE_MAX_IMAGE_SIZE)
        return MB_PE_FILE_SOURCE_INVALID_ARGUMENT;
    do { fd = open(profile->path,O_RDONLY | O_NONBLOCK | O_NOFOLLOW); }
    while (fd < 0 && errno == EINTR);
    if (fd < 0) return MB_PE_FILE_SOURCE_OPEN_ERROR;
    do { rc = fstat(fd,&st); } while (rc < 0 && errno == EINTR);
    if (rc < 0) result = MB_PE_FILE_SOURCE_IO_ERROR;
    else if (!S_ISREG(st.st_mode)) result = MB_PE_FILE_SOURCE_NOT_REGULAR;
    else if (st.st_size <= 0 || (uintmax_t)st.st_size > profile->max_bytes)
        result = MB_PE_FILE_SOURCE_SIZE_ERROR;
    if (result != MB_PE_FILE_SOURCE_OK) {
        (void)close(fd);
        return result;
    }
    bytes = (size_t)st.st_size;
    snapshot = (unsigned char *)malloc(bytes);
    if (!snapshot) result = MB_PE_FILE_SOURCE_NO_MEMORY;
    while (result == MB_PE_FILE_SOURCE_OK && used < bytes) {
        ssize_t got = read(fd,snapshot + used,bytes - used);
        if (got < 0 && errno == EINTR) continue;
        if (got <= 0) { result = MB_PE_FILE_SOURCE_IO_ERROR; break; }
        used += (size_t)got;
    }
    if (close(fd) < 0) result = MB_PE_FILE_SOURCE_IO_ERROR;
    if (result != MB_PE_FILE_SOURCE_OK) {
        free(snapshot);
        return result;
    }
    *file = snapshot;
    *size = bytes;
    *token = snapshot;
    return MB_PE_FILE_SOURCE_OK;
}
int mb_pe_file_source_release(void *context, void *token) {
    (void)context;
    if (!token) return MB_PE_FILE_SOURCE_INVALID_ARGUMENT;
    free(token);
    return MB_PE_FILE_SOURCE_OK;
}
