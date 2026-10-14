#pragma once
#include <stddef.h>
#define MFA_OK 0
typedef struct mfa_config mfa_config;
struct mfa_config { const char *root; const char *title_id; };
typedef struct mfa_error mfa_error;
struct mfa_error { char message[128]; char path[256]; int os_error; };
typedef struct mfa_activation mfa_activation;
struct mfa_activation;
typedef struct mfa_status mfa_status;
struct mfa_status { int fd; int reserved; };
int mfa_activate(const struct mfa_config *, struct mfa_activation **, struct mfa_error *);
const char *mfa_result_name(int);
unsigned long long mfa_preloaded_bytes(struct mfa_activation *);
int mfa_status_open(struct mfa_status *, const char *, long, unsigned long long);
void mfa_status_write(struct mfa_status *, const char *, int, const char *);
void mfa_status_close(struct mfa_status *);
