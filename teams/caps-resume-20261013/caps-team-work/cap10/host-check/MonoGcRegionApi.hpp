#pragma once
// Exact public declarations excerpted from pinned Mono mono-threads-api.h.
// MDK omits experimental header; full runtime header requires build config.h.
// No invented ABI, empty GC implementation or manufactured Mono config.
#include <glib.h>
#include <mono/utils/mono-publib.h>
MONO_BEGIN_DECLS
MONO_API MONO_RT_EXTERNAL_ONLY gpointer
mono_threads_enter_gc_unsafe_region (gpointer* stackdata);
MONO_API MONO_RT_EXTERNAL_ONLY void
mono_threads_exit_gc_unsafe_region (gpointer cookie, gpointer* stackdata);
MONO_END_DECLS
