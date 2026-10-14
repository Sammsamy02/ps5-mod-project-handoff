#define _POSIX_C_SOURCE 200809L
#define _DARWIN_C_SOURCE
#include "../pe_file_source.h"
#include <fcntl.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/stat.h>
#include <unistd.h>
#define CHECK(x) do { if (!(x)) { fprintf(stderr,"line%d: %s\n",__LINE__,#x); exit(1); } } while (0)
static void write_file(const char *path, const void *data, size_t bytes) {
    FILE *f = fopen(path,"wb");
    CHECK(f && fwrite(data,1,bytes,f) == bytes && fclose(f) == 0);
}
static void refused(mb_pe_loader_catalog_entry *entry, int expected) {
    const void *file = (void *)1; size_t bytes = 99; void *token = (void *)2;
    CHECK(mb_pe_file_source_acquire(NULL,entry,&file,&bytes,&token) == expected);
    CHECK(file == NULL && bytes == 0 && token == NULL);
}
int main(void) {
    char root[] = "/tmp/mb-pe-file-source-XXXXXX";
    char path[256],other[256],link_path[256],fifo[256];
    static const unsigned char original[] = {0,1,2,0xff,'P','E',0,'x'};
    const void *file = NULL; size_t bytes = 0; void *token = NULL;
    mb_pe_file_source_profile profile = {path,64};
    mb_pe_loader_catalog_entry entry = {0}; entry.source_context = &profile;
    CHECK(mkdtemp(root));
    CHECK(snprintf(path,sizeof(path),"%s/module.dll",root) > 0);
    CHECK(snprintf(other,sizeof(other),"%s/replacement",root) > 0);
    CHECK(snprintf(link_path,sizeof(link_path),"%s/link.dll",root) > 0);
    CHECK(snprintf(fifo,sizeof(fifo),"%s/fifo",root) > 0);
    write_file(path,original,sizeof(original));
    CHECK(mb_pe_file_source_acquire(NULL,&entry,&file,&bytes,&token) == MB_PE_FILE_SOURCE_OK);
    CHECK(file && token && bytes == sizeof(original) && memcmp(file,original,bytes) == 0);
    write_file(other,"replacement content",19);
    CHECK(rename(other,path) == 0);
    CHECK(bytes == sizeof(original) && memcmp(file,original,bytes) == 0);
    CHECK(mb_pe_file_source_release(NULL,token) == 0);
    token = NULL; file = NULL;
    CHECK(mb_pe_file_source_acquire(NULL,&entry,&file,&bytes,&token) == MB_PE_FILE_SOURCE_OK);
    CHECK(bytes == 19 && memcmp(file,"replacement content",bytes) == 0);
    CHECK(mb_pe_file_source_release(NULL,token) == 0);
    profile.max_bytes = 18; refused(&entry,MB_PE_FILE_SOURCE_SIZE_ERROR);
    profile.max_bytes = 19;
    CHECK(mb_pe_file_source_acquire(NULL,&entry,&file,&bytes,&token) == 0);
    CHECK(bytes == 19); CHECK(mb_pe_file_source_release(NULL,token) == 0);
    write_file(path,"",0); refused(&entry,MB_PE_FILE_SOURCE_SIZE_ERROR);
    CHECK(unlink(path) == 0); refused(&entry,MB_PE_FILE_SOURCE_OPEN_ERROR);
    profile.path = root; refused(&entry,MB_PE_FILE_SOURCE_NOT_REGULAR);
    profile.path = path; write_file(path,original,sizeof(original));
    CHECK(symlink(path,link_path) == 0); profile.path = link_path;
    refused(&entry,MB_PE_FILE_SOURCE_OPEN_ERROR);
    CHECK(mkfifo(fifo,0600) == 0); profile.path = fifo;
    refused(&entry,MB_PE_FILE_SOURCE_NOT_REGULAR);
    profile.path = path; profile.max_bytes = 0;
    refused(&entry,MB_PE_FILE_SOURCE_INVALID_ARGUMENT);
    profile.max_bytes = (size_t)MB_PE_MAX_IMAGE_SIZE + 1;
    refused(&entry,MB_PE_FILE_SOURCE_INVALID_ARGUMENT);
    profile.max_bytes = 64; profile.path = NULL;
    refused(&entry,MB_PE_FILE_SOURCE_INVALID_ARGUMENT);
    profile.path = ""; refused(&entry,MB_PE_FILE_SOURCE_INVALID_ARGUMENT);
    entry.source_context = NULL; refused(&entry,MB_PE_FILE_SOURCE_INVALID_ARGUMENT);
    refused(NULL,MB_PE_FILE_SOURCE_INVALID_ARGUMENT);
    entry.source_context = &profile; profile.path = path;
    file = (void *)1; bytes = 99;
    CHECK(mb_pe_file_source_acquire(NULL,&entry,&file,&bytes,NULL) == MB_PE_FILE_SOURCE_INVALID_ARGUMENT);
    CHECK(file == NULL && bytes == 0);
    CHECK(mb_pe_file_source_release(NULL,NULL) == MB_PE_FILE_SOURCE_INVALID_ARGUMENT);
    CHECK(unlink(fifo) == 0 && unlink(link_path) == 0 && unlink(path) == 0 && rmdir(root) == 0);
    puts("file-source: actual regular-file acquisition, immutable snapshots, replacement, size/type/path and argument refusals passed");
    return 0;
}
