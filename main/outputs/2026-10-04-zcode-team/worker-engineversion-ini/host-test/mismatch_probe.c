/* worker-engineversion-ini negative probe: NEW loadlist row (834) vs OLD staged
 * settings bytes (779). The runtime chain performs exactly mfa_activate below
 * (mirrors test_activation_final.c T2). Expected: MFA_E_SIZE (-8). */
#include "modforge_activation.h"
#include <stdio.h>
#include <string.h>
int main(int argc, char **argv) {
    (void)argc;
    mfa_activation *a = NULL;
    mfa_error err;
    memset(&err, 0, sizeof err);
    mfa_config cfg;
    memset(&cfg, 0, sizeof cfg);
    cfg.root = argv[1];
    cfg.title_id = "PPSA33286";
    int rc = mfa_activate(&cfg, &a, &err);
    printf("rc=%d err.code=%d err.message=%s err.path=%s\n", rc, err.code, err.message, err.path);
    return rc == MFA_E_SIZE && err.code == -8 ? 0 : 1;
}
