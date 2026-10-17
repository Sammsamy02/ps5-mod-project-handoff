/* A genuine compiler/linker PE input, with named and ordinal imports plus a
 * DIR64 relocation. It is consumed as immutable bytes, never executed. */
__declspec(dllimport) unsigned Named(unsigned);
__declspec(dllimport) unsigned Ordinal(unsigned);
__declspec(dllexport) unsigned Entry(unsigned value) { return Named(value) + Ordinal(value); }
__declspec(dllexport) void *Anchor = (void *)&Entry;
