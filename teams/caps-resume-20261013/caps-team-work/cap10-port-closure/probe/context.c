#include <config.h>
#include <glib.h>
#include <mono/utils/mono-context.h>
void capture_context(MonoContext *context) { MONO_CONTEXT_GET_CURRENT(*context); }
