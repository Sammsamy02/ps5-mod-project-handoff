/* SPDX-License-Identifier: GPL-3.0-or-later
 * Based on ps5-payload-sdk v0.43 samples/browser/main.c.
 * Copyright (C) 2023 John Törnblom
 * ModBridge: fixed local app URL and explicit service error handling.
 */
#include <stdio.h>
#include <stdlib.h>

/* These declarations and libraries follow the pinned SDK browser sample. */
int sceUserServiceInitialize(void *);
int sceUserServiceTerminate(void);
int sceSystemServiceLaunchWebBrowser(const char *uri, void *);

static const char app_url[] = "http://127.0.0.1:8085/app";

int main(void)
{
    int rc = sceUserServiceInitialize(NULL);
    if (rc != 0) {
        fprintf(stderr, "ModBridge: UserService startup failed (0x%08x)\n",
                (unsigned int)rc);
        return EXIT_FAILURE;
    }

    const int launch_rc = sceSystemServiceLaunchWebBrowser(app_url, NULL);
    if (launch_rc != 0) {
        fprintf(stderr, "ModBridge: browser launch failed (0x%08x)\n",
                (unsigned int)launch_rc);
    }

    rc = sceUserServiceTerminate();
    if (rc != 0) {
        fprintf(stderr, "ModBridge: UserService shutdown failed (0x%08x)\n",
                (unsigned int)rc);
    }
    return launch_rc == 0 && rc == 0 ? EXIT_SUCCESS : EXIT_FAILURE;
}
