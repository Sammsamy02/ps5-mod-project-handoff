/* Direct test of the genuine configured Mono runtime loader; no copied implementation. */
#include "config.h"
#include "mono/utils/mono-dl.h"
#include <glib.h>
#include <stdio.h>
#include <string.h>
#include <stdlib.h>
#include <unistd.h>
struct Provider { const char *name; int loads, symbols, closes, reject; };
static int supplied_value(void) { return 7; }
static void *provider_load(const char *name, int flags, char **err, void *user)
{
    struct Provider *p = user;
    (void)flags; (void)err;
    p->loads++;
    return !p->reject && name && !strcmp(name, p->name) ? p : NULL;
}
static void *provider_symbol(void *handle, const char *name, char **err, void *user)
{
    struct Provider *p = user;
    p->symbols++;
    if (handle == p && !strcmp(name, "cap10_loader_sentinel")) return (void *)&supplied_value;
    if (err) *err = g_strdup("provider rejected unknown symbol");
    return NULL;
}
static void *provider_close(void *handle, void *user)
{
    struct Provider *p = user;
    if (handle == p) p->closes++;
    return NULL;
}
static void need(int ok, const char *reason)
{
    if (!ok) { fprintf(stderr, "FAIL %s\n", reason); exit(1); }
}
static int read_symbol(MonoDl *module)
{
    void *ptr = NULL;
    char *err = mono_dl_symbol(module, "cap10_loader_sentinel", &ptr);
    need(!err && ptr, "expected symbol");
    return ((int (*)(void))ptr)();
}
static int marker_exists(void)
{
    const char *path = getenv("CAP10_AMBIENT_MARKER");
    need(path != NULL, "marker path supplied");
    return access(path, F_OK) == 0;
}
int main(int argc, char **argv)
{
    char *err = NULL;
    MonoDl *module;
    need(argc == 3, "mode and fixture path");
    if (!strcmp(argv[1], "deny")) {
        module = mono_dl_open(argv[2], MONO_DL_LOCAL, &err);
        if (module) { printf("AMBIENT_ADMITTED value=%d marker=%d\n", read_symbol(module), marker_exists()); mono_dl_close(module); fflush(stdout); }
        need(module == NULL, "reserved name refused without provider");
        need(err && !strcmp(err, "Owned library has no registered provider"), "strict owned error");
        need(!marker_exists(), "ambient constructor never executed");
        g_free(err);
    } else if (!strcmp(argv[1], "normal")) {
        module = mono_dl_open(argv[2], MONO_DL_LOCAL, &err);
        need(module != NULL, "ordinary library still loads");
        need(read_symbol(module) == 99 && marker_exists(), "ordinary genuine OS library used");
        mono_dl_close(module);
    } else if (!strcmp(argv[1], "provider")) {
        struct Provider p = { argv[2], 0, 0, 0, 0 };
        MonoDlFallbackHandler *handler = mono_dl_fallback_register(provider_load, provider_symbol, provider_close, &p);
        need(handler != NULL, "provider registered");
        module = mono_dl_open(argv[2], MONO_DL_LOCAL, &err);
        need(module != NULL, "owned provider load");
        need(read_symbol(module) == 7 && p.loads == 1 && p.symbols == 1, "provider used before existing ambient library");
        need(!marker_exists(), "provider path never loaded ambient library");
        void *missing = (void *)1;
        char *symbol_error = mono_dl_symbol(module, "unknown_symbol", &missing);
        need(symbol_error && missing == NULL, "provider unknown symbol refused");
        g_free(symbol_error);
        mono_dl_close(module);
        need(p.closes == 1, "provider survives through module close");
        mono_dl_fallback_unregister(handler);
    } else if (!strcmp(argv[1], "unregister")) {
        for (int i = 0; i < 32; ++i) {
            struct Provider first = { argv[2], 0, 0, 0, 0 }, head = { argv[2], 0, 0, 0, 1 };
            MonoDlFallbackHandler *first_handle = mono_dl_fallback_register(provider_load, provider_symbol, provider_close, &first);
            MonoDlFallbackHandler *head_handle = mono_dl_fallback_register(provider_load, provider_symbol, provider_close, &head);
            need(first_handle && head_handle, "both handlers registered");
            mono_dl_fallback_unregister(head_handle);
            module = mono_dl_open(argv[2], MONO_DL_LOCAL, &err);
            need(module && read_symbol(module) == 7 && first.loads == 1 && head.loads == 0, "lookup after head unregister reaches surviving handler");
            mono_dl_close(module);
            need(first.closes == 1, "surviving provider closed before unregister");
            mono_dl_fallback_unregister(first_handle);
            module = mono_dl_open(argv[2], MONO_DL_LOCAL, &err);
            need(!module && err, "lookup after last unregister is refused");
            g_free(err); err = NULL;
        }
        need(!marker_exists(), "unregister tests never loaded ambient library");
    } else need(0, "recognized mode");
    printf("REAL_MONO_LOADER_PASS mode=%s\n", argv[1]);
    return 0;
}
